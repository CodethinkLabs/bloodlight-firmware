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

#include "host/common/msg.h"
#include "host/common/sig.h"

#include "fifo.h"
#include "util.h"

// DEFAULT_AVERAGE_WIDTH, the number of milliseconds to average over
long DEFAULT_AVERAGE_WIDTH = 1000;

struct channel_data {
	bl_msg_sample_data_t msg; // message to buffer output
	struct fifo *samples; // all the samples in the rolling sum
	uint64_t rolling_sum; // sum across the samples that will be averaged
	uint32_t baseline; // "zero" level for an "average" sample
};

void add_normalized_sample(bl_msg_sample_data_t *msg, uint32_t sample)
{
	// sample may be 16-bit or 32-bit, depending on data format.
	if (msg->type == BL_MSG_SAMPLE_DATA16) {
		msg->data16[msg->count] = (uint16_t) sample;
		msg->count++;
	} else {
		msg->data32[msg->count] = sample;
		msg->count++;
	}
	if ((msg->type == BL_MSG_SAMPLE_DATA16
	     && msg->count == MSG_SAMPLE_DATA16_MAX)
	    || (msg->type == BL_MSG_SAMPLE_DATA32
	        && msg->count == MSG_SAMPLE_DATA32_MAX)) {
			// Message is full, send and start refilling
			bl_msg_yaml_print(stdout, (union bl_msg_data *) msg);
			msg->count = 0;
	}
}

int add_sample(long average_width,
		struct channel_data *channel,
		uint32_t sample)
{
	// Add a sample to the fifo
	if (!fifo_write(channel->samples, sample)) {
		fprintf(stderr, "FIFO overflow\n");
		return -ERANGE;
	}
	// Add to the rolling sum
	channel->rolling_sum += sample;
	if (channel->samples->used >= average_width) {
		uint32_t new_sample, old_sample;
		uint32_t normalized_sample;

		// Calculate the normalized sample
	   	if (!fifo_peek_back(channel->samples,
					average_width / 2,
					&new_sample)) {
			fprintf(stderr, "FIFO underflow\n");
			return -ERANGE;
		}
		normalized_sample = channel->baseline + new_sample
		                    - (channel->rolling_sum / average_width);
		add_normalized_sample(&channel->msg, (uint32_t) normalized_sample);

		// Subtract from the rolling sum
		if (!fifo_read(channel->samples, &old_sample)) {
			fprintf(stderr, "Tried to read from empty FIFO\n");
			return -ENOENT;
		}
		channel->rolling_sum -= old_sample;
	}
	return 0;
	
}

void init_channel(struct channel_data *channel,
		const bl_msg_channel_conf_t *msg)
{
	memset(channel, 0, sizeof(*channel));
	if (msg->sample32) {
		channel->msg.type = BL_MSG_SAMPLE_DATA32;
		channel->baseline = INT32_MAX;
	} else {
		channel->msg.type = BL_MSG_SAMPLE_DATA16;
		channel->baseline = INT16_MAX;
	}
	channel->msg.channel = msg->channel;

}

void destroy_channel(struct channel_data *channel)
{
	if (channel->samples != NULL) {
		fifo_destroy(channel->samples);
	}
}

int read_stream(FILE *stream, long average_width)
{
	union bl_msg_data msg; // message for reading into
	struct channel_data channels[BL_CHANNEL_MAX] = {0};
	struct channel_data *channel;
	long average_width_samples = 0; // Average width measured in samples
	while (!bl_sig_killed && bl_msg_yaml_parse(stream, &msg)) {
		switch(msg.type) {
		case BL_MSG_CHANNEL_CONF:
			/* Doesn't create the channel's fifos yet, since we don't know how
			 * many samples we need to store until we know the frequency */
			init_channel(channels + msg.channel_conf.channel,
					&msg.channel_conf);
			bl_msg_yaml_print(stdout, &msg);
			break;
		case BL_MSG_START:
			average_width_samples = average_width * msg.start.frequency
				/ 1000; // Magical 1000 from being measured in milliseconds.

			// Create all the channels' fifos now
			for (unsigned i = 0; i < BL_ARRAY_LEN(channels); i++) {
				channels[i].samples = fifo_create(average_width_samples);
				if (channels[i].samples == NULL) {
					return -errno;
				}
			}
			bl_msg_yaml_print(stdout, &msg);
			break;
		case BL_MSG_SAMPLE_DATA16:
		case BL_MSG_SAMPLE_DATA32:
			channel = channels + msg.sample_data.channel;

			if (channel->msg.type != msg.type) {
				fprintf(stderr,
					"Error: Sample data for channel %"PRIu8" has an unexpected"
					" type (expected %d, got %d)\n", msg.sample_data.channel,
					channel->msg.type, msg.type);
				return -1;
			}

			for (unsigned i = 0; i < msg.sample_data.count; i++) {
				int ret;
				if (msg.type == BL_MSG_SAMPLE_DATA16) {
					ret = add_sample(average_width_samples, channel,
							msg.sample_data.data16[i]);
				} else {
					ret = add_sample(average_width_samples, channel,
							msg.sample_data.data32[i]);
				}
				if (ret < 0) {
					return ret;
				}
			}
			break;
		default:
			bl_msg_yaml_print(stdout, &msg);
		}
	}
	for (unsigned i = 0; i < BL_ARRAY_LEN(channels); i++) {
		destroy_channel(channels + i);
	}
	return 0;
}

void usage(FILE* file, char *argv[])
{
	fprintf(file, "Usage: %s [AVERAGE_WIDTH]\n", argv[0]);
	fprintf(file, "  AVERAGE_WIDTH: The time (in ms) to average over\n");
}

int main(int argc, char *argv[])
{
	uint32_t average_width = DEFAULT_AVERAGE_WIDTH;
	enum {
		ARG_PROG_NAME,
		ARG_AVERAGE_WIDTH,

		ARG__COUNT,
	};
	/* Reads from stdin, writes to stdout */
	/* The only argument is averaging width, which has a default */
	if (argc > ARG__COUNT) {
		fprintf(stderr, "%d is the wrong number of arguments\n", argc);
		usage(stderr, argv);
		return EXIT_FAILURE;
	}

	// Parse average_width
	if (argc == ARG__COUNT) {
		if (read_sized_uint(argv[1], &average_width, sizeof(average_width))) {
		} else {
			fprintf(stderr, "Could not parse '%s'\n", argv[ARG_AVERAGE_WIDTH]);
			usage(stderr, argv);
			return EXIT_FAILURE;
		}
	}

	if (!bl_sig_init()) {
		return EXIT_FAILURE;
	}

	return read_stream(stdin, average_width);
}
