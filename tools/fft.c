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
#include <fftw3.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "common/msg.h"
#include "common/channel.h"
#include "common/util.h"

#include "msg.h"
#include "sig.h"
#include "util.h"
#include "fifo.h"

uint32_t DEFAULT_WINDOW_LENGTH = 1000; // measured in milliseconds
// Decided to have a fixed window interval of half the sample window.
uint16_t DEFAULT_WINDOW_COUNT = 3; // number of windows to average together

struct sample_window {
	double *in_buffer;
	fftw_complex *out_buffer;
	uint32_t buffer_capacity;
	uint32_t out_capacity;
	uint32_t sample_count;
	fftw_plan plan;
	bool transformed;
};

struct channel_data {
	uint16_t channel;
	struct pfifo *windows;
	uint32_t welch_window_count;
	uint32_t windows_capacity;
	uint32_t window_length;
	uint32_t new_window_interval; // Samples before opening a window
	uint32_t sample_index; // Samples since last opening a window
	double *welch_output;
	uint64_t output_index; // Distinguishes subsequent fourier transforms
};

static void destroy_sample_window(struct sample_window *window)
{
	if (window != NULL) {
		if (window->plan != NULL) {
			fftw_destroy_plan(window->plan);
		}
		if (window->out_buffer != NULL) {
			fftw_free(window->out_buffer);
		}
		free(window->in_buffer);
		free(window);
	}
}

static struct sample_window *create_sample_window(
		const struct channel_data *channel)
{
	struct sample_window *window = calloc(1, sizeof(*window));
	if (window == NULL) {
		return NULL;
	}
	window->buffer_capacity = channel->window_length;
	window->out_capacity = channel->window_length / 2 + 1;
	window->in_buffer = malloc(window->buffer_capacity *
			sizeof(*window->in_buffer));
	if (window->in_buffer == NULL) {
		goto cleanup;
	}
	window->out_buffer = fftw_malloc((window->out_capacity) *
			sizeof(*window->out_buffer));
	if (window->out_buffer == NULL) {
		goto cleanup;
	}
	window->plan = fftw_plan_dft_r2c_1d(channel->window_length,
			window->in_buffer, window->out_buffer, FFTW_MEASURE);
	if (window->plan == NULL) {
		goto cleanup;
	}
	return window;

cleanup:
		destroy_sample_window(window);
		return NULL;
}

static inline bool window_is_full(struct sample_window *window)
{
	return window->sample_count >= window->buffer_capacity;
}

static unsigned add_sample_to_window(struct sample_window *window, double sample)
{
	if (!window_is_full(window)) {
		window->in_buffer[window->sample_count] = sample;
		window->sample_count++;
	}
	if (window_is_full(window)) {
		if (!window->transformed) {
			fftw_execute(window->plan);
		}
		return 1;
	}
	return 0;
}

static inline void mod_welch_average(struct channel_data *channel,
		unsigned window_value, struct sample_window *window, int mod)
{
	for (uint32_t i = 0; i < window->out_capacity; i++) {
		double real, imag, mag, addition;
		real = window->out_buffer[i][0];
		imag = window->out_buffer[i][1];
		mag = real * real + imag * imag;
		// complex magnitude is immediately squared in welch's method
		addition = mag / window_value / channel->welch_window_count;

		channel->welch_output[i] += mod * addition;
	}
}

static inline void add_to_welch_average(struct channel_data *channel,
		unsigned window_value, struct sample_window *window)
{
	mod_welch_average(channel, window_value, window, 1);
}

static inline void subtract_from_welch_average(struct channel_data *channel,
		unsigned window_value, struct sample_window *window)
{
	mod_welch_average(channel, window_value, window, -1);
}

static void welch_method(struct channel_data *channel)
{
	// Unless we use a non-square window function, the window value is easy
	// Otherwise, sum the windowed value squared over the entire window
	unsigned window_value = channel->window_length;
	unsigned out_length = channel->window_length / 2 + 1;
	struct sample_window *window;
	if (channel->welch_output == NULL) {
		// Populate from every full window if this is the first run
		channel->welch_output = calloc(out_length,
				sizeof(*channel->welch_output));

		for (unsigned i = 0; pfifo_peek_back(channel->windows, i,
					(void**) &window); i++) {
			if (window_is_full(window)) {
				add_to_welch_average(channel, window_value, window);
			}
		}
	} else {
		// Get the most recent full window
		for (unsigned i = 0; pfifo_peek_back(channel->windows, i,
					(void**) &window); i++) {
			if (window_is_full(window)) {
				break;
			}
		}
		add_to_welch_average(channel, window_value, window);
	}
}

static void print_welch_output(struct channel_data *channel)
{
	unsigned out_length = channel->window_length / 2 + 1;
	for (unsigned i = 0; i < out_length; i++) {
		printf("%"PRIu16",%"PRIu64",%f\n", channel->channel,
				channel->output_index, channel->welch_output[i]);
	}
	channel->output_index++;
}

static int add_sample_to_channel(struct channel_data *channel, double sample)
{
	unsigned full_windows = 0;
	struct sample_window *window;
	if (channel->sample_index > 0
			&& channel->sample_index % channel->new_window_interval == 0) {
		struct sample_window *w = create_sample_window(channel);
		if (w == NULL) {
			return -errno;
		}
		if (!pfifo_write(channel->windows, w)) {
			return -1;
		}
		channel->sample_index = 0;
	}

	for (unsigned i = 0; pfifo_peek_back(channel->windows, i,
				(void**) &window); i++) {
		full_windows += add_sample_to_window(window, sample);
	}

	if (full_windows == channel->welch_window_count) {
		welch_method(channel);
		print_welch_output(channel);

		if (!pfifo_read(channel->windows, (void**) &window)) {
			return -1;
		}
		// TODO: centralise calculation of window_value
		subtract_from_welch_average(channel, channel->window_length, window);
		destroy_sample_window(window);
	}


	channel->sample_index++;
	return 0;
}

static int init_channel(struct channel_data *channel,
		const bl_msg_channel_conf_t *msg, uint32_t window_count)
{
	memset(channel, 0, sizeof(*channel));
	channel->channel = msg->channel;

	/* I have decided to always use 50% overlap
	 * If the window length in samples is even-numbered, there'd be 1
	 * partly-full window when we have window_count full windows.
	 * With odd-numbered window length, there's a second newly-started window
	 */
	channel->windows_capacity = window_count + 2;
	channel->welch_window_count = window_count;
	channel->windows = pfifo_create(channel->windows_capacity);
	if (channel->windows == NULL) {
		return -errno;
	}
	return 0;
}

static int init_channel_samples(struct channel_data *channel,
		uint32_t window_length_samples)
{
	struct sample_window *window;
	channel->window_length = window_length_samples;
	channel->new_window_interval = window_length_samples / 2;
	if (channel->windows == NULL) {
		fprintf(stderr, "Failed to initialise channel. "
				"Perhaps a channel config message is missing from the input "
				"stream\n");
		return -1;
	}
	window = create_sample_window(channel);
	if (window == NULL) {
		return -errno;
	}
	if (!pfifo_write(channel->windows, window)) {
		return -1;
	}
	return 0;
}

static void destroy_channel(struct channel_data *channel)
{
	struct sample_window *window;
	if (channel->welch_output != NULL) {
		free(channel->welch_output);
	}
	// Destroy all existing windows
	if (channel->windows != NULL) {
		while (pfifo_read(channel->windows, (void**) &window)) {
			destroy_sample_window(window);
		}
		pfifo_destroy(channel->windows);
	}
}

static int read_stream(uint32_t window_length, uint16_t window_count)
{
	union bl_msg_data msg; // message for reading into
	struct channel_data channels[BL_CHANNEL_MAX] = {0};
	struct channel_data *channel;
	int ret;
	unsigned highest_channel = 0;

	while (!bl_sig_killed && bl_msg_yaml_parse(stdin, &msg)) {
		uint32_t length_samples;
		switch(msg.type) {
		case BL_MSG_CHANNEL_CONF:
			assert(msg.channel_conf.channel <
					BL_ARRAY_LEN(channels));

			if (msg.channel_conf.channel > highest_channel) {
				highest_channel = msg.channel_conf.channel;
			}

			ret = init_channel(channels + msg.channel_conf.channel,
					&msg.channel_conf, window_count);
			if (ret < 0) {
				goto cleanup;
			}

			break;
		case BL_MSG_START:
			length_samples = window_length *
				msg.start.frequency	/ 1000; // Magical 1000 from milliseconds.

			// Create all the channels' buffers now we know the size
			for (unsigned i = 0; i <= highest_channel; i++) {
				ret = init_channel_samples(channels + i, length_samples);
				if (ret < 0) {
					goto cleanup;
				}
			}
			break;
		case BL_MSG_SAMPLE_DATA16:
		case BL_MSG_SAMPLE_DATA32:
			assert(msg.sample_data.channel <
					BL_ARRAY_LEN(channels));
			channel = channels + msg.sample_data.channel;

			for (unsigned i = 0; i < msg.sample_data.count; i++) {
				if (msg.type == BL_MSG_SAMPLE_DATA16) {
					add_sample_to_channel(channel,
							(double) msg.sample_data.data16[i]);
				} else {
					add_sample_to_channel(channel,
							(double) msg.sample_data.data32[i]);
				}
			}
			break;
		}
	}
	ret = 0;
cleanup:
	for (unsigned i = 0; i < BL_ARRAY_LEN(channels); i++) {
		destroy_channel(channels + i);
	}
	return ret;
}

static void usage(FILE* file, char *argv[])
{
	fprintf(file, "Performs fourier transforms on a continuous stream of data\n");
	fprintf(file, "This is done by taking the fourier transform of a series "
			"of overlapping windows and averaging them together\n");
	fprintf(file, "It outputs a series of transforms in the format "
			"[transform_index],[channel_id],[value]\n");
	fprintf(file, "\n");
	fprintf(file, "Usage: %s [WINDOW_LENGTH] [WINDOW_COUNT]\n", argv[0]);
	fprintf(file, "  SAMPLE_WINDOW: The time (in ms) to perform the fft over\n");
	fprintf(file, "  WINDOW COUNT: The number of windows to average over\n");
}

int main(int argc, char *argv[])
{
	uint32_t window_length = DEFAULT_WINDOW_LENGTH;
	uint32_t window_count = DEFAULT_WINDOW_COUNT;
	enum {
		ARG_PROG_NAME,
		ARG_WINDOW_LENGTH,
		ARG_WINDOW_COUNT,

		ARG__COUNT,
	};
	if (argc > ARG__COUNT) {
		fprintf(stderr, "%d is the wrong number of arguments\n", argc);
		usage(stderr, argv);
		return EXIT_FAILURE;
	}

	// Parse window length
	if (argc > ARG_WINDOW_LENGTH) {
		if (read_sized_uint(argv[ARG_WINDOW_LENGTH], &window_length,
					sizeof(window_length))) {
		} else {
			fprintf(stderr, "Could not parse '%s'\n", argv[ARG_WINDOW_LENGTH]);
			usage(stderr, argv);
			return EXIT_FAILURE;
		}
	}

	if (argc > ARG_WINDOW_COUNT) {
		if (read_sized_uint(argv[ARG_WINDOW_COUNT], &window_count,
					sizeof(window_count))) {
		} else {
			fprintf(stderr, "Could not parse '%s'\n", argv[ARG_WINDOW_COUNT]);
			usage(stderr, argv);
			return EXIT_FAILURE;
		}
	}

	if (!bl_sig_init()) {
		return EXIT_FAILURE;
	}

	return read_stream(window_length, window_count);
}
