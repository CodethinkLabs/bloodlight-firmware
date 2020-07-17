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
volatile bool killed;

/** Max USB packet size in bytes. */
#define USB_PACKET_MAX 64

/** Storage for incomming messages to read into. */
union {
	union bl_msg_data msg;
	uint8_t raw[USB_PACKET_MAX];
} input;

struct channel_conf {
	bool     enabled;
	uint8_t  gain;
	uint8_t  shift;
	uint32_t offset;
	uint8_t  saturate;
};

static void bl__calibrate_channel(
		unsigned channel,
		uint32_t min,
		uint32_t max,
		uint8_t bits,
		struct channel_conf *conf)
{
	uint32_t max_range    = (1U << bits) - 1;
	uint32_t margin       = max_range / 4;
	uint32_t target_range = max_range - (margin * 2);
	uint32_t range        = max - min;
	uint32_t shift        = 0;
	uint32_t offset;

	while ((range >> shift) > target_range) {
		shift++;
	}

	offset = (margin < min) ? (min - margin) : 0;

	printf("Channel: %u\n", channel);
	if (min > max) {
		if ((min == 0xFFFFFFFF) && (max == 0)) {
			printf("    Disabled\n");
		} else {
			printf("    Range:  %"PRIu32" - %"PRIu32" (INVERTED)\n", min, max);
		}
	} else {
		printf("    Range:  %"PRIu32" (%"PRIu32"..%"PRIu32")\n", range, min, max);
		printf("    Offset: %"PRIu32"\n", offset);
		printf("    Shift:  %"PRIu32"\n", shift);
	}

	conf->offset = offset;
	conf->shift  = shift;
}

static void bl__handle_aborted(
		const union bl_msg_data *msg,
		struct channel_conf conf[BL_ACQ__SRC_COUNT])
{
	const uint32_t *sample_min = msg->aborted.sample_min;
	const uint32_t *sample_max = msg->aborted.sample_max;

	for (unsigned i = 0; i < BL_ACQ__SRC_COUNT; i++) {
		bl__calibrate_channel(i, sample_min[i], sample_max[i], 16, &conf[i]);
	}
}

static void bl__handle_start(
		const union bl_msg_data *msg,
		struct channel_conf conf[BL_ACQ__SRC_COUNT])
{
	for (unsigned i = 0; i < BL_ACQ__SRC_COUNT; i++) {
		if ((1U << i) & msg->start.src_mask) {
			conf[i].enabled = true;
		}
	}
}

static void bl__handle_channel_conf(
		const union bl_msg_data *msg,
		struct channel_conf conf[BL_ACQ__SRC_COUNT])
{
	unsigned channel = msg->channel_conf.channel;

	conf[channel].gain     = msg->channel_conf.gain;
	conf[channel].shift    = msg->channel_conf.shift;
	conf[channel].offset   = msg->channel_conf.offset;
	conf[channel].saturate = msg->channel_conf.saturate;
}

static int bl__calibrate(const char *dev_path)
{
	struct channel_conf conf[BL_ACQ__SRC_COUNT] = { 0 };
	union bl_msg_data *msg = &input.msg;

	while (bl_msg_parse(msg)) {
		switch (msg->type) {
		case BL_MSG_START:
			bl__handle_start(msg, conf);
			break;
		case BL_MSG_ABORTED:
			bl__handle_aborted(msg, conf);
			break;
		case BL_MSG_CHANNEL_CONF:
			bl__handle_channel_conf(msg, conf);
			break;
		case BL_MSG_SAMPLE_DATA:
			/* Ignore because it's noisy. */
			break;
		default:
			/* Print so we can see what's going on. */
			bl_msg_print(msg, stdout);
			break;
		}
	}

	for (unsigned i = 0; i < BL_ACQ__SRC_COUNT; i++) {
		if (conf[i].enabled) {
			printf("./tools/bl chancfg %s %u %u %u %u %u\n",
					dev_path, i,
					(unsigned) conf[i].gain,
					(unsigned) conf[i].offset,
					(unsigned) conf[i].shift,
					(unsigned) conf[i].saturate);
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

