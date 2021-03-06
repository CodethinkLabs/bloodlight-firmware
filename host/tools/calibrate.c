/*
 * Copyright 2020 Codethink Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <inttypes.h>
#include <stdbool.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "common/error.h"
#include "common/util.h"
#include "common/msg.h"

#include "host/common/msg.h"
#include "host/common/sig.h"

struct channel_conf {
	bool     enabled;

	uint8_t  source;
	uint8_t  shift;
	uint32_t offset;

	uint32_t sample_min, sample_max;
};

static void bl__calibrate_channel(
		unsigned channel,
		uint8_t bits,
		struct channel_conf *conf)
{
	uint32_t max_range    = (1U << bits) - 1;
	uint32_t margin       = max_range / 4;
	uint32_t target_range = max_range - (margin * 2);
	uint32_t range        = conf->sample_max - conf->sample_min;
	uint32_t shift        = 0;
	uint32_t offset;

	while ((range >> shift) > target_range) {
		shift++;
	}

	offset = (margin < conf->sample_min) ? (conf->sample_min - margin) : 0;

	printf("Channel: %u\n", channel);
	if (conf->sample_min > conf->sample_max) {
		if ((conf->sample_min == 0xFFFFFFFF) &&
				(conf->sample_max == 0)) {
			printf("    Disabled\n");
		} else {
			printf("    Range:  %"PRIu32" - %"PRIu32" (INVERTED)\n",
					conf->sample_min, conf->sample_max);
		}
	} else {
		printf("    Range:  %"PRIu32" (%"PRIu32"..%"PRIu32")\n",
				range, conf->sample_min, conf->sample_max);
		printf("    Offset: %"PRIu32"\n", offset);
		printf("    Shift:  %"PRIu32"\n", shift);
	}

	conf->offset = offset;
	conf->shift  = shift;
}

static void bl__handle_start(
		const union bl_msg_data *msg,
		struct channel_conf conf[BL_ACQ_SOURCE_MAX])
{
	(void) msg;

	/* Re-start calibration on start message. */
	for (unsigned i = 0; i < BL_ACQ_SOURCE_MAX; i++) {
		conf[i].sample_min = 0xFFFFFFFF;
		conf[i].sample_max = 0x00000000;
	}
}

static void bl__handle_channel_conf(
		const union bl_msg_data *msg,
		struct channel_conf conf[BL_ACQ_SOURCE_MAX])
{
	unsigned channel = msg->channel_conf.channel;

	/* Channel is only enabled if we receive sample32. */
	conf[channel].enabled = false;

	conf[channel].source = msg->channel_conf.source;
	conf[channel].shift  = msg->channel_conf.shift;
	conf[channel].offset = msg->channel_conf.offset;

	/* Set these here in-case we never see a start. */
	conf[channel].sample_min = 0xFFFFFFFF;
	conf[channel].sample_max = 0x00000000;
}

static void bl__handle_sample_data32(
		const union bl_msg_data *msg,
		struct channel_conf conf[BL_ACQ_SOURCE_MAX])
{
	unsigned channel = msg->sample_data.channel;
	if (channel >= BL_ACQ_SOURCE_MAX) {
		return;
	}

	for (unsigned i = 0; i < msg->sample_data.count; i++) {
		uint32_t sample = msg->sample_data.data32[i];
		if (sample < conf[channel].sample_min) {
			conf[channel].sample_min = sample;
		}
		if (sample > conf[channel].sample_max) {
			conf[channel].sample_max = sample;
		}
	}

	/* If we're receiving 32-bit samples it's enabled. */
	conf[channel].enabled = true;
}

static int bl__calibrate(void)
{
	struct channel_conf conf[BL_ACQ_SOURCE_MAX] = { 0 };
	union bl_msg_data msg;

	while (!bl_sig_killed && bl_msg_yaml_parse(stdin, &msg)) {
		switch (msg.type) {
		case BL_MSG_START:
			bl__handle_start(&msg, conf);
			break;
		case BL_MSG_CHANNEL_CONF:
			bl__handle_channel_conf(&msg, conf);
			break;
		case BL_MSG_SAMPLE_DATA16:
			/* Ignore because it's noisy. */
			break;
		case BL_MSG_SAMPLE_DATA32:
			bl__handle_sample_data32(&msg, conf);
			break;
		default:
			/* Print so we can see what's going on. */
			bl_msg_yaml_print(stdout, &msg);
			break;
		}
	}

	for (unsigned i = 0; i < BL_ARRAY_LEN(conf); i++) {
		if (conf[i].enabled) {
			bl__calibrate_channel(i, 16, &conf[i]);
		}
	}

	for (unsigned i = 0; i < BL_ARRAY_LEN(conf); i++) {
		if (conf[i].enabled) {
			printf("./tools/bl chancfg \"$device\" %u %u %u %u\n", i,
					(unsigned) conf[i].source,
					(unsigned) conf[i].offset,
					(unsigned) conf[i].shift);
		}
	}

	return EXIT_SUCCESS;
}

static void bl__help(const char *prog)
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "  %s", prog);
	fprintf(stderr, "\n");
	fprintf(stderr, "Reads an aqcuisition log message from stdin, and prints\n");
	fprintf(stderr, "suggested acquisition parameters to stdout\n");
}

int main(int argc, char *argv[])
{
	enum {
		ARG_PROG,
		ARG__COUNT,
	};

	if (argc != ARG__COUNT) {
		bl__help(argv[ARG_PROG]);
		return EXIT_FAILURE;
	}

	if (!bl_sig_init()) {
		return EXIT_FAILURE;
	}

	return bl__calibrate();
}
