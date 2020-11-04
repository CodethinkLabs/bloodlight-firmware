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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>

#include "common/msg.h"
#include "common/channel.h"
#include "common/util.h"

#include "msg.h"
#include "sig.h"
#include "util.h"

struct channel_data {
	uint8_t channel;
	uint16_t frequency;
	uint64_t sample_index;
	uint64_t old_peak_index;
	uint32_t old_peak_height;
	uint64_t new_peak_index;
	uint32_t new_peak_height;
	bool in_peak;
};

uint32_t DEFAULT_PEAK_THRESHOLD = 3 * (UINT32_MAX / 4); // half-way above the middle.

static inline double calculate_bpm(const struct channel_data *channel)
{
	return 60.0 * (double) channel->frequency /
		(double) (channel->new_peak_index - channel->old_peak_index);
}

void channel_process_sample(struct channel_data *channel,
		uint32_t peak_threshold, const bl_msg_sample_data_t *msg)
{
	uint32_t value;
	for (uint8_t i = 0; i < msg->count; i++) {
		if (msg->type == BL_MSG_SAMPLE_DATA16) {
			/* Upscale 16-bit samples to 32-bit. */
			value = msg->data16[i];
			value = (value << 16) | value;
		} else {
			value = msg->data32[i];
		}

		if (value >= peak_threshold) {
			if (!channel->in_peak) {
				// Entered a new peak
				channel->in_peak = true;
			}
			if (value > channel->new_peak_height) {
				channel->new_peak_height = value;
				channel->new_peak_index = channel->sample_index;
			}

		} else if (channel->in_peak) {
			// Just left a peak
			channel->in_peak = false;
			if (channel->old_peak_height) {
				// a previous peak exists to compare times for
				printf("%u,%u\n", channel->channel,
						(unsigned) calculate_bpm(channel));
			}
			channel->old_peak_index = channel->new_peak_index;
			channel->old_peak_height = channel->new_peak_height;
			channel->new_peak_index = 0;
			channel->new_peak_height = 0;
		}
		channel->sample_index++;
	}
}

void channel_start(struct channel_data *channel,
		const bl_msg_start_t *msg)
{
	channel->frequency = msg->frequency;
}

void init_channel(struct channel_data *channel,
		const bl_msg_channel_conf_t *msg)
{
	channel->channel = msg->channel;
}

int read_stream(uint32_t peak_threshold)
{
	union bl_msg_data msg; // message for reading into
	struct channel_data channels[BL_CHANNEL_MAX] = {0};
	struct channel_data *channel;
	while (!bl_sig_killed && bl_msg_yaml_parse(stdin, &msg)) {
		switch (msg.type) {
		case BL_MSG_CHANNEL_CONF:
			init_channel(channels + msg.channel_conf.channel,
					&msg.channel_conf);
			break;
		case BL_MSG_START:
			for (unsigned i = 0; i < BL_ARRAY_LEN(channels); i++) {
				channel_start(channels + i, &msg.start);
			}
			break;
		case BL_MSG_SAMPLE_DATA16:
		case BL_MSG_SAMPLE_DATA32:

			channel = channels + msg.sample_data.channel;

			channel_process_sample(channel, peak_threshold,
					&msg.sample_data);
			break;
		}
	}
	return EXIT_SUCCESS;
}

void usage(FILE* file, char *argv[])
{
	fprintf(file, "Calculates beats-per-minute for a message stream by "
			"finding the time the signal above a certain threshold reached "
			"its highest point\n");
	fprintf(file, "This threshold may need tweaking for particularly weak "
			"signals\n");
	fprintf(file, "The default threshold is %"PRIu32"\n",
			DEFAULT_PEAK_THRESHOLD);
	fprintf(file, "\n");
	fprintf(file, "Usage: %s [PEAK_THRESHOLD]\n", argv[0]);
	fprintf(file, "  PEAK_THRESHOLD: threshold above which to search for "
			"peaks\n");
}

int main(int argc, char *argv[])
{
	uint32_t peak_threshold = DEFAULT_PEAK_THRESHOLD;
	enum {
		ARG_PROG_NAME,
		ARG_PEAK_THRESHOLD,

		ARG__COUNT,
	};
	if (argc > ARG__COUNT) {
		fprintf(stderr, "%d is the wrong number of arguments\n", argc);
		usage(stderr, argv);
		return EXIT_FAILURE;
	}

	if (argc == ARG__COUNT) {
		if (read_sized_uint(argv[ARG_PEAK_THRESHOLD], &peak_threshold,
					sizeof(peak_threshold))) {
		} else {
			fprintf(stderr, "Could not parse '%s'\n",
					argv[ARG_PEAK_THRESHOLD]);
			usage(stderr, argv);
			return EXIT_FAILURE;
		}
	}

	if (!bl_sig_init()) {
		return EXIT_FAILURE;
	}

	return read_stream(peak_threshold);
}
