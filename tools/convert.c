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

#include <stdbool.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "../src/error.h"
#include "../src/util.h"
#include "../src/msg.h"

/* Common helper functionality. */
#include "common.c"

#define MAX_SAMPLES 64
#define MAX_CHANNELS 16

#define BUFFER_LEN 64
#define BUFFER_LEN_STR "64"

static char buffer[BUFFER_LEN + 1];

typedef int (* bl_cmd_fn)(int argc, char *argv[]);

static inline enum bl_msg_type bl_msg_str_to_type(const char *str)
{
	static const char *types[] = {
		[BL_MSG_RESPONSE]    = "Response",
		[BL_MSG_LED_TEST]    = "LED Test",
		[BL_MSG_ACQ_SETUP]   = "Acq Setup",
		[BL_MSG_ACQ_START]   = "Acq Start",
		[BL_MSG_ACQ_ABORT]   = "Acq Abort",
		[BL_MSG_RESET]       = "Reset",
		[BL_MSG_SAMPLE_DATA] = "Sample Data",
	};

	if (str != NULL) {
		for (unsigned i = 0; i < BL_ARRAY_LEN(types); i++) {
			if (types[i] != NULL) {
				if (strcmp(types[i], str) == 0) {
					return i;
				}
			}
		}
	}

	return BL_MSG__COUNT;
}

static const char * read_str_type(bool *success)
{
	int ret;

	ret = scanf("- %"BUFFER_LEN_STR"[A-Za-z0-9 ]:\n", buffer);
	if (ret != 1) {
		*success = false;
		return NULL;
	}

	return buffer;
}

static enum bl_msg_type read_type(bool *success)
{
	const char *str_type = read_str_type(success);

	return bl_msg_str_to_type(str_type);
}

static uint8_t read_response_to(bool *success)
{
	unsigned value;
	int ret;

	ret = scanf("    Response to: %"BUFFER_LEN_STR"[A-Za-z0-9 ]\n",
			buffer);
	if (ret == 1) {
		return bl_msg_str_to_type(buffer);
	}

	ret = scanf("    Response to: Unknown (0x%x)\n", &value);
	if (ret == 1) {
		return value;
	}

	*success = false;
	return bl_msg_str_to_type("Unknown");
}

static uint16_t read_unsigned(bool *success, const char *field)
{
	unsigned value;
	int ret;

	ret = scanf("%"BUFFER_LEN_STR"[A-Za-z0-9 ]: %u\n",
			buffer, &value);
	if (ret == 2) {
		if (strcmp(buffer, field) == 0) {
			return value;
		}
	}

	*success = false;
	return 0;
}

static uint16_t read_hex(bool *success, const char *field)
{
	unsigned value;
	int ret;

	ret = scanf("%"BUFFER_LEN_STR"[A-Za-z0-9 ]: 0x%x\n",
			buffer, &value);
	if (ret == 2) {
		if (strcmp(buffer, field) == 0) {
			return value;
		}
	}

	*success = false;
	return 0;
}

static uint16_t read_unsigned_no_field(bool *success)
{
	unsigned value;
	int ret;

	ret = scanf("%*[ -]%u\n", &value);
	if (ret == 1) {
		return value;
	}

	*success = false;
	return 0;
}

static bool read_message(union bl_msg_data *msg)
{
	bool ok = true;
	msg->type = read_type(&ok);

	switch (msg->type) {
	case BL_MSG_RESPONSE:
		msg->response.response_to = read_response_to(&ok);
		msg->response.error_code = read_unsigned(&ok, "Error code");
		break;

	case BL_MSG_LED_TEST:
		msg->led_test.led_mask = read_hex(&ok, "LED Mask");
		break;

	case BL_MSG_ACQ_SETUP:
		msg->acq_setup.oversample = read_unsigned(&ok, "Oversample");
		msg->acq_setup.rate = read_unsigned(&ok, "Rate");
		msg->acq_setup.src_mask = read_hex(&ok, "Source Mask");
		ok |= (scanf("    Gain:\n") == 0);
		for (unsigned i = 0; i < BL_ACQ_PD__COUNT; i++) {
			msg->acq_setup.gain[i] = read_unsigned_no_field(&ok);
		}
		break;

	case BL_MSG_SAMPLE_DATA:
		msg->sample_data.count = read_unsigned(&ok, "Count");
		msg->sample_data.src_mask = read_hex(&ok, "Source Mask");
		ok |= (scanf("    Data:\n") == 0);
		for (unsigned i = 0; i < msg->sample_data.count; i++) {
			msg->sample_data.data[i] = read_unsigned_no_field(&ok);
		}
		break;

	default:
		break;
	}

	return ok;
}

static uint32_t bl_msg_get_sample_rate(uint32_t period_us)
{
	return (1 * 1000 * 1000) / period_us;
}

static uint16_t bl_msg_get_num_channels(uint16_t src_mask)
{
	uint16_t bit_count = 0;

	for (unsigned i = 0; i < 16; i++) {
		if (src_mask & (1 << i)) {
			bit_count++;
		}
	}

	return bit_count;
}

static bool bl_masks_to_channel_idxs(
		uint16_t acq_src_mask,
		uint16_t *restrict msg_src_mask,
		uint16_t *restrict acq_idx)
{
	uint16_t acq_channel = 0;

	for (unsigned i = 0; i < 16; i++) {
		uint16_t bit = (1 << i);

		if (acq_src_mask & bit) {
			acq_channel++;
			if (*msg_src_mask & bit) {
				*msg_src_mask &= ~bit;
				*acq_idx = acq_channel - 1;
				return true;
			}
		}
	}

	return false;
}

static inline int32_t bl_sample_to_signed(uint16_t in)
{
	return in - INT16_MIN;
}

static unsigned bl_msg_copy_samples(
		union bl_msg_data *msg,
		uint16_t acq_src_mask,
		int16_t data[MAX_SAMPLES * MAX_CHANNELS],
		uint16_t channel_counter[MAX_CHANNELS])
{
	uint16_t msg_channel_count = bl_msg_get_num_channels(msg->sample_data.src_mask);
	uint16_t acq_channel_count = bl_msg_get_num_channels(acq_src_mask);
	uint16_t msg_sample_count = msg->sample_data.count;
	uint16_t msg_src_mask = msg->sample_data.src_mask;
	uint16_t acq_idx = 0;
	uint16_t msg_idx = 0;
	uint16_t counter = 0;
	uint16_t check_mask;

	while (bl_masks_to_channel_idxs(acq_src_mask, &msg_src_mask, &acq_idx)) {
		size_t data_off = acq_channel_count * channel_counter[acq_idx] +
				acq_idx;

		for (unsigned i = msg_idx; i < msg_sample_count; i += msg_channel_count) {
			data[data_off] = bl_sample_to_signed(msg->sample_data.data[i]);
			data_off += acq_channel_count;
			channel_counter[acq_idx]++;
		}
		msg_idx++;
	}

	acq_idx = 0;
	msg_idx = 0;
	check_mask = acq_src_mask;
	while (bl_masks_to_channel_idxs(acq_src_mask, &check_mask, &acq_idx)) {
		if (channel_counter[acq_idx] == 0) {
			return 0;
		} else if (counter != 0 &&
				counter != channel_counter[acq_idx]) {
			return 0;
		}
		counter = channel_counter[acq_idx];
	}

	return counter;
}

/* Note: We'll break if there are more than MAX_SAMPLES in a message. */
union {
	union bl_msg_data msg;
	uint8_t samples[sizeof(union bl_msg_data) + sizeof(uint16_t) * MAX_SAMPLES];
} input;

int bl_cmd_wav(int argc, char *argv[])
{
	uint16_t channel_counter[MAX_CHANNELS] = { 0 };
	union bl_msg_data *msg = &input.msg;
	bool had_setup = false;
	uint16_t acq_src_mask;
	size_t written;
	FILE *file;
	enum {
		ARG_PROG,
		ARG_CMD,
		ARG_PATH,
		ARG__COUNT,
	};
	struct {
		char chunk_id[4];
		uint32_t chunk_size;
		char format[4];
	} riff_header = {
		.chunk_id = { 'R', 'I', 'F', 'F' },
		.format   = { 'W', 'A', 'V', 'E' },
		.chunk_size= 0xffffffff,
	};
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
		.bits_per_sample = 16,
	};
	struct {
		char subchunk_id[4];
		uint32_t subchunk_size;
		int16_t data[MAX_SAMPLES * MAX_CHANNELS];
	} wave_data = {
		.subchunk_id = { 'd', 'a', 't', 'a' },
	};

	if (argc < ARG_PATH || argc > ARG__COUNT) {
		fprintf(stderr, "Usage:\n");
		fprintf(stderr, "  %s %s [PATH]\n",
				argv[ARG_PROG], argv[ARG_CMD]);
		fprintf(stderr, "\n");
		fprintf(stderr, "If no PATH is given, WAVE data will be written to stdout.\n");
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

	written = fwrite(&riff_header, sizeof(riff_header), 1, file);
	if (written != 1) {
		fprintf(stderr, "Failed to write riff_header header\n");
		goto cleanup;
	}

	while (read_message(msg)) {
		unsigned samples_copied;

		if (!had_setup && msg->type == BL_MSG_ACQ_SETUP) {
			wave_format.sample_rate = bl_msg_get_sample_rate(
					msg->acq_setup.rate);
			wave_format.num_channels = bl_msg_get_num_channels(
					msg->acq_setup.src_mask);
			wave_format.byte_rate =
					wave_format.sample_rate *
					wave_format.num_channels *
					wave_format.bits_per_sample;
			wave_format.block_align =
					wave_format.num_channels *
					wave_format.bits_per_sample / 8;
			acq_src_mask = msg->acq_setup.src_mask;
			had_setup = true;

			written = fwrite(&wave_format, sizeof(wave_format), 1, file);
			if (written != 1) {
				fprintf(stderr, "Failed to write wave_format header\n");
				goto cleanup;
			}
		}

		/* If the message isn't sample data, print to stderr, so
		 * the user can see what's going on. */
		if (msg->type != BL_MSG_SAMPLE_DATA) {
			bl_msg_print(msg, stderr);
			continue;
		}

		if (!had_setup) {
			fprintf(stderr, "No acq_setup message found\n");
			goto cleanup;
		}

		samples_copied = bl_msg_copy_samples(msg, acq_src_mask,
				wave_data.data, channel_counter);
		if (samples_copied > 0) {
			wave_data.subchunk_size =
					bl_msg_get_num_channels(acq_src_mask) *
					samples_copied * sizeof(int16_t);
			written = fwrite(&wave_data,
					8 + wave_data.subchunk_size, 1, file);
			if (written != 1) {
				fprintf(stderr, "Failed to write wave_data\n");
				goto cleanup;
			}
			memset(channel_counter, 0, sizeof(channel_counter));
		}
	}

cleanup:
	if (argc == ARG__COUNT) {
		fclose(file);
	}

	return EXIT_SUCCESS;
}

int bl_cmd_relay(int argc, char *argv[])
{
	union bl_msg_data *msg = &input.msg;
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

	while (read_message(msg)) {
		bl_msg_print(msg, stdout);
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

	return cmd_fn(argc, argv);
}

