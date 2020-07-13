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

#include <signal.h>

#include "../src/error.h"
#include "../src/util.h"
#include "../src/msg.h"

/* Common helper functionality. */
#include "msg.h"
#include "find_device.h"

/** Whether we've had a `ctrl-c`. */
static volatile bool killed = false;

struct channel_conf {
	bool     enabled;

	uint8_t  gain;
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
		struct channel_conf conf[BL_ACQ__SRC_COUNT])
{
	(void) msg;

	/* Re-start calibration on start message. */
	for (unsigned i = 0; i < BL_ACQ__SRC_COUNT; i++) {
		conf[i].sample_min = 0xFFFFFFFF;
		conf[i].sample_max = 0x00000000;
	}
}

static void bl__handle_channel_conf(
		const union bl_msg_data *msg,
		struct channel_conf conf[BL_ACQ__SRC_COUNT])
{
	unsigned channel = msg->channel_conf.channel;

	/* Channel is only enabled if we receive sample32. */
	conf[channel].enabled = false;

	conf[channel].gain   = msg->channel_conf.gain;
	conf[channel].shift  = msg->channel_conf.shift;
	conf[channel].offset = msg->channel_conf.offset;

	/* Set these here in-case we never see a start. */
	conf[channel].sample_min = 0xFFFFFFFF;
	conf[channel].sample_max = 0x00000000;
}

static void bl__handle_sample_data32(
		const union bl_msg_data *msg,
		struct channel_conf conf[BL_ACQ__SRC_COUNT])
{
	unsigned channel = msg->sample_data.channel;

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

static int bl__calibrate(const char *dev_path)
{
	struct channel_conf conf[BL_ACQ__SRC_COUNT] = { 0 };
	union bl_msg_data msg;

	while (!killed && bl_msg_parse(&msg)) {
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
			bl_msg_print(&msg, stdout);
			break;
		}
	}

	for (unsigned i = 0; i < BL_ACQ__SRC_COUNT; i++) {
		if (conf[i].enabled) {
			bl__calibrate_channel(i, 16, &conf[i]);

			printf("./tools/bl chancfg %s %u %u %u %u\n",
					dev_path, i,
					(unsigned) conf[i].gain,
					(unsigned) conf[i].offset,
					(unsigned) conf[i].shift);
		}
	}

	return EXIT_SUCCESS;
}

static void bl__help(const char *prog)
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "  %s <DEV_PATH|auto|--auto|-a>\n", prog);
	fprintf(stderr, "\n");
	fprintf(stderr, "Reads an aqcuisition log message from stdin, and prints\n");
	fprintf(stderr, "suggested acquisition parameters to stdout\n");
}

static void bl__ctrl_c_handler(int sig)
{
	if (sig == SIGINT) {
		killed = true;
	}
}

static int bl__setup_signal_handler(void)
{
	struct sigaction act = {
		.sa_handler = bl__ctrl_c_handler,
	};
	int ret;

	ret = sigemptyset(&act.sa_mask);
	if (ret == -1) {
		fprintf(stderr, "sigemptyset call failed: %s\n",
				strerror(errno));
		return EXIT_FAILURE;
	}

	ret = sigaddset(&act.sa_mask, SIGINT);
	if (ret == -1) {
		fprintf(stderr, "sigaddset call failed: %s\n",
				strerror(errno));
		return EXIT_FAILURE;
	}

	ret = sigaction(SIGINT, &act, NULL);
	if (ret == -1) {
		fprintf(stderr, "Failed to set SIGINT handler: %s\n",
				strerror(errno));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
	enum {
		ARG_PROG,
		ARG_DEV_PATH,
		ARG__COUNT,
	};

	if (argc < ARG__COUNT) {
		bl__help(argv[ARG_PROG]);
		return EXIT_FAILURE;
	}

	if (bl__setup_signal_handler() != EXIT_SUCCESS) {
		return EXIT_FAILURE;
	}

	get_dev(ARG_DEV_PATH, argv);

	return bl__calibrate(argv[ARG_DEV_PATH]);
}

