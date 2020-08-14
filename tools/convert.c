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
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "../src/error.h"
#include "../src/util.h"
#include "../src/msg.h"

#include "msg.h"
#include "sig.h"
#include "fifo.h"

#define FIFO_MAX 1024

typedef int (* bl_cmd_fn)(int argc, char *argv[]);

static uint16_t bl_count_channels(uint16_t src_mask)
{
	uint16_t bit_count = 0;

	for (unsigned i = 0; i < 16; i++) {
		if (src_mask & (1 << i)) {
			bit_count++;
		}
	}

	return bit_count;
}

static inline int32_t bl_sample_to_signed(uint32_t in)
{
	return in - INT32_MIN;
}

enum bl_format {
	BL_FORMAT_WAV,
	BL_FORMAT_RAW,
	BL_FORMAT_CSV,
};

static int bl_cmd_wav_write_riff_header(FILE *file)
{
	size_t written;
	struct {
		char chunk_id[4];
		uint32_t chunk_size;
		char format[4];
	} riff_header = {
		.chunk_id   = { 'R', 'I', 'F', 'F' },
		.format     = { 'W', 'A', 'V', 'E' },
		.chunk_size = 0xffffffff, /* Trick value for "streaming" samples */
	};

	written = fwrite(&riff_header, sizeof(riff_header), 1, file);
	if (written != 1) {
		fprintf(stderr, "Failed to write riff_header header\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static int bl_cmd_wav_write_format_header(
		FILE *file,
		uint16_t frequency,
		uint16_t src_mask)
{
	size_t written;
	struct {
		char subchunk_id[4];
		uint32_t subchunk_size;
		uint16_t audio_format;
		uint16_t num_channels;
		uint32_t sample_rate;
		uint32_t byte_rate;
		uint16_t block_align;
		uint16_t bits_per_sample;
	} wave_format = {
		.subchunk_id = { 'f', 'm', 't', ' ' },
		.subchunk_size = 16, /* For PCM format. */
		.audio_format = 1, /* PCM format. */
		.bits_per_sample = 32,
		.sample_rate = frequency,
		.num_channels = bl_count_channels(src_mask),
	};

	wave_format.byte_rate = wave_format.sample_rate *
			wave_format.num_channels *
			wave_format.bits_per_sample;
	wave_format.block_align = wave_format.num_channels *
			wave_format.bits_per_sample / 8;

	written = fwrite(&wave_format, sizeof(wave_format), 1, file);
	if (written != 1) {
		fprintf(stderr, "Failed to write wave_format header\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static int bl_cmd_wav_write_data_header(FILE *file)
{
	size_t written;
	struct {
		char subchunk_id[4];
		uint32_t subchunk_size;
	} wave_data = {
		.subchunk_id = { 'd', 'a', 't', 'a' },
		.subchunk_size = 0xffffffff, /* Trick value for "streaming" samples */
	};

	written = fwrite(&wave_data, sizeof(wave_data), 1, file);
	if (written != 1) {
		fprintf(stderr, "Failed to write wave_data header\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static uint8_t chan[BL_ACQ__SRC_COUNT] = { 0 };

static void init_chan_lookup(unsigned src_mask)
{
	unsigned used = 0;

	for (unsigned i = 0; i < BL_ACQ__SRC_COUNT; i++) {
		if ((1U << i) & src_mask) {
			chan[i] = used;
			used++;
		}
	}
}

static int bl_sample_msg_to_file(
		FILE *file,
		unsigned frequency,
		union bl_msg_data *msg,
		unsigned src_mask,
		unsigned num_channels,
		enum bl_format format,
		struct fifo **fifos)
{
	static unsigned curr_channel = 0;

	unsigned count = msg->sample_data.count;

	if (format == BL_FORMAT_CSV) {
		static unsigned counter;

		float period = 1000.0 / frequency;
		for (unsigned s = 0; s < count; s++) {
			uint32_t sample = msg->type == BL_MSG_SAMPLE_DATA16 ?
					msg->sample_data.data16[s] :
					msg->sample_data.data32[s];

			float x_ms = period * counter;
			unsigned c = curr_channel++;
			fprintf(file, "%d,%f,%"PRIu32"\n", c, x_ms, sample);
			if (curr_channel >= num_channels) {
				curr_channel = 0;
			}
			counter++;
		}
	} else {
		size_t written;

		uint32_t value;
		for (unsigned i = 0; i < count; i++) {
			if (msg->type == BL_MSG_SAMPLE_DATA16) {
				/* Upscale 16-bit samples to 32-bit. */
				value = msg->sample_data.data16[i];
				value = (value << 16) | value;
			} else {
				value = msg->sample_data.data32[i];
			}

			if (format == BL_FORMAT_WAV) {
				value = (uint32_t)bl_sample_to_signed(value);
			}

			if (!fifo_write(fifos[chan[msg->sample_data.channel]], value)) {
				fprintf(stderr, "FIFO overflow\n");
				return EXIT_FAILURE;
			}
		}

		unsigned ready = UINT_MAX;
		for (unsigned i = 0; i < num_channels; i++) {
			if (fifos[i]->used < ready) {
				ready = fifos[i]->used;
			}
		}

		for (unsigned i = 0; i < ready; i++) {
			for (unsigned j = 0; j < num_channels; j++) {
				fifo_read(fifos[j], &value);
				written = fwrite(&value, sizeof(value), 1, file);
				if (written != 1) {
					fprintf(stderr, "Failed to write wave_data\n");
					return EXIT_FAILURE;
				}
			}
		}
	}

	fflush(file);

	return EXIT_SUCCESS;
}

static int bl_samples_to_file(int argc, char *argv[], enum bl_format format)
{
	union bl_msg_data msg;
	unsigned num_channels = 0;
	unsigned src_mask = 0;
	bool had_setup = false;
	unsigned frequency;
	FILE *file;
	int ret;
	struct fifo *fifos[BL_ACQ__SRC_COUNT];
	enum {
		ARG_PROG,
		ARG_CMD,
		ARG_PATH,
		ARG__COUNT,
	};

	if (argc < ARG_PATH || argc > ARG__COUNT) {
		fprintf(stderr, "Usage:\n");
		fprintf(stderr, "  %s %s [PATH]\n",
				argv[ARG_PROG], argv[ARG_CMD]);
		fprintf(stderr, "\n");
		fprintf(stderr, "If no PATH is given, data will be written to stdout.\n");
		return EXIT_FAILURE;
	}

	if (argc == ARG__COUNT) {
		file = fopen(argv[ARG_PATH], "w");
		if (file == NULL) {
			fprintf(stderr, "Failed to open '%s': %s\n",
					argv[ARG_PATH], strerror(errno));
			return EXIT_FAILURE;
		}
	} else {
		file = stdout;
	}

	for (unsigned i = 0; i < BL_ACQ__SRC_COUNT; i++) {
		fifos[i] = fifo_create(FIFO_MAX);
		if (fifos[i] == NULL) {
			fprintf(stderr, "Failed to create fifo: %s\n",
					strerror(errno));
			return EXIT_FAILURE;
		}
	}

	if (format == BL_FORMAT_WAV) {
		ret = bl_cmd_wav_write_riff_header(file);
		if (ret != EXIT_SUCCESS) {
			goto cleanup;
		}
	}

	while (!bl_sig_killed && bl_msg_yaml_parse(stdin, &msg)) {
		if (!had_setup && msg.type == BL_MSG_START) {
			src_mask = msg.start.src_mask;
			num_channels = bl_count_channels(src_mask);
			init_chan_lookup(src_mask);
			had_setup = true;

			frequency = msg.start.frequency;

			if (format == BL_FORMAT_WAV) {
				ret = bl_cmd_wav_write_format_header(file,
						msg.start.frequency,
						msg.start.src_mask);
				if (ret != EXIT_SUCCESS) {
					goto cleanup;
				}
				ret = bl_cmd_wav_write_data_header(file);
				if (ret != EXIT_SUCCESS) {
					goto cleanup;
				}
			} else {
				fprintf(stderr, "- RAW output format:\n");
				fprintf(stderr, "    Samples: 32-bit signed\n");
				fprintf(stderr, "    Channels: %u\n", num_channels);
				fprintf(stderr, "    Frequency: %u Hz\n", frequency);
			}
		}

		/* If the message isn't sample data, print to stderr, so
		 * the user can see what's going on. */
		if (msg.type != BL_MSG_SAMPLE_DATA16 &&
		    msg.type != BL_MSG_SAMPLE_DATA32) {
			bl_msg_yaml_print(stderr, &msg);
			continue;
		}

		if (!had_setup) {
			fprintf(stderr, "No acq_setup message found\n");
			goto cleanup;
		}

		ret = bl_sample_msg_to_file(file, frequency, &msg, src_mask,
				num_channels, format, fifos);
		if (ret != EXIT_SUCCESS) {
			goto cleanup;
		}
	}
cleanup:
	for (unsigned i = 0; i < BL_ACQ__SRC_COUNT; i++) {
		fifo_destroy(fifos[i]);
	}

	if (argc == ARG__COUNT) {
		fclose(file);
	}

	return EXIT_SUCCESS;
}

static int bl_cmd_wav(int argc, char *argv[])
{
	return bl_samples_to_file(argc, argv, BL_FORMAT_WAV);
}

static int bl_cmd_raw(int argc, char *argv[])
{
	return bl_samples_to_file(argc, argv, BL_FORMAT_RAW);
}

static int bl_cmd_csv(int argc, char *argv[])
{
	return bl_samples_to_file(argc, argv, BL_FORMAT_CSV);
}

static int bl_cmd_relay(int argc, char *argv[])
{
	union bl_msg_data msg;
	enum {
		ARG_PROG,
		ARG_CMD,
		ARG__COUNT,
	};

	if (argc != ARG__COUNT) {
		fprintf(stderr, "Usage:\n");
		fprintf(stderr, "  %s %s\n",
				argv[ARG_PROG], argv[ARG_CMD]);
		return EXIT_FAILURE;
	}

	while (!bl_sig_killed && bl_msg_yaml_parse(stdin, &msg)) {
		bl_msg_yaml_print(stdout, &msg);
	}

	return EXIT_SUCCESS;
}

static const struct bl_cmd {
	const char *name;
	const char *help;
	const bl_cmd_fn fn;
} cmds[] = {
	{
		.name = "wav",
		.help = "Convert to WAVE format",
		.fn = bl_cmd_wav,
	},
	{
		.name = "raw",
		.help = "Convert to RAW binary data",
		.fn = bl_cmd_raw,
	},
	{
		.name = "csv",
		.help = "Convert to CSV",
		.fn = bl_cmd_csv,
	},
	{
		.name = "relay",
		.help = "Relay stdin to stdout",
		.fn = bl_cmd_relay,
	},
};

static void bl_cmd_help(const char *prog)
{
	unsigned max_name = 0;

	for (unsigned i = 0; i < BL_ARRAY_LEN(cmds); i++) {
		unsigned len = strlen(cmds[i].name);
		if (len > max_name) {
			max_name = len;
		}
	}

	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "  %s CMD [params]\n", prog);
	fprintf(stderr, "\n");

	fprintf(stderr, "Available CMDs:\n");
	for (unsigned i = 0; i < BL_ARRAY_LEN(cmds); i++) {
		fprintf(stderr, "  %-*s   %s\n", max_name,
				cmds[i].name,
				cmds[i].help);
	}
	fprintf(stderr, "\n");
}

static bl_cmd_fn bl_cmd_lookup(const char *cmd_name)
{
	for (unsigned i = 0; i < BL_ARRAY_LEN(cmds); i++) {
		if (strcmp(cmds[i].name, cmd_name) == 0) {
			return cmds[i].fn;
		}
	}

	return NULL;
}

int main(int argc, char *argv[])
{
	bl_cmd_fn cmd_fn;
	enum {
		ARG_PROG,
		ARG_CMD,
		ARG__COUNT,
	};

	if (argc < ARG__COUNT) {
		bl_cmd_help(argv[ARG_PROG]);
		return EXIT_FAILURE;
	}

	cmd_fn = bl_cmd_lookup(argv[ARG_CMD]);
	if (cmd_fn == NULL) {
		bl_cmd_help(argv[ARG_PROG]);
		return EXIT_FAILURE;
	}

	if (!bl_sig_init()) {
		return EXIT_FAILURE;
	}

	return cmd_fn(argc, argv);
}

