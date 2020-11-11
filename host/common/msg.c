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
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>

#include <unistd.h>
#include <poll.h>

#include "common/error.h"
#include "common/util.h"
#include "common/msg.h"

#include "msg.h"
#include "sig.h"

#define BUFFER_LEN 64
#define BUFFER_LEN_STR "64"

static char buffer[BUFFER_LEN + 1];

/** Message type to string mapping, */
static const char *msg_types[]  = {
	[BL_MSG_RESPONSE]       = "Response",
	[BL_MSG_LED]            = "LED",
	[BL_MSG_SOURCE_CONF]    = "Source Config",
	[BL_MSG_CHANNEL_CONF]   = "Channel Config",
	[BL_MSG_START]          = "Start",
	[BL_MSG_ABORT]          = "Abort",
	[BL_MSG_SAMPLE_DATA16]  = "Sample Data 16-bit",
	[BL_MSG_SAMPLE_DATA32]  = "Sample Data 32-bit",
	[BL_MSG_SOURCE_CAP_REQ] = "Source Capability Request",
	[BL_MSG_SOURCE_CAP]     = "Source Capability",
};

/** Message type to string mapping, */
static const char *msg_errors[] = {
	[BL_ERROR_NONE]                = "Success",
	[BL_ERROR_OUT_OF_RANGE]        = "Value out of range",
	[BL_ERROR_BAD_MESSAGE_TYPE]    = "Bad message type",
	[BL_ERROR_BAD_MESSAGE_LENGTH]  = "Bad message length",
	[BL_ERROR_BAD_SOURCE_MASK]     = "Bad source mask",
	[BL_ERROR_MODE_MISMATCH]       = "The acquisition mode mismatches with led_mask",
	[BL_ERROR_ACTIVE_ACQUISITION]  = "In acquisition state",
	[BL_ERROR_BAD_FREQUENCY]       = "Unsupported frequency combination",
	[BL_ERROR_NOT_IMPLEMENTED]     = "Feature not implemented",
	[BL_ERROR_HARDWARE_CONFLICT]   = "Hardware conflict",
	[BL_ERROR_ADC_FREQ_TOO_HIGH]   = "Frequency too high for ADC",
	[BL_ERROR_ADC_DMA_BUFFER]      = "Config exceeds DMA buffer size",
	[BL_ERROR_DAC_BAD_CHANNEL]     = "Bad DAC channel",
	[BL_ERROR_DAC_BAD_OFFSET]      = "Bad DAC offset",
	[BL_ERROR_OPAMP_BAD_GAIN]      = "Bad opamp gain",
	[BL_ERROR_TIMER_BAD_FREQUENCY] = "Bad timer frequency",
};

static inline unsigned bl_msg_str_to_index(
		const char *str,
		const char * const *strings,
		unsigned count)
{
	if (str != NULL) {
		for (unsigned i = 0; i < count; i++) {
			if (strings[i] != NULL) {
				if (strcmp(strings[i], str) == 0) {
					return i;
				}
			}
		}
	}

	return count;
}

static inline enum bl_msg_type bl_msg_str_to_type(const char *str)
{
	return bl_msg_str_to_index(str, msg_types, BL_ARRAY_LEN(msg_types));
}

static inline enum bl_error bl_msg_str_to_error(const char *str)
{
	return bl_msg_str_to_index(str, msg_errors, BL_ARRAY_LEN(msg_errors));
}

static const char * bl_msg__yaml_read_str_type(FILE *file, bool *success)
{
	int ret;

	ret = fscanf(file, "- %"BUFFER_LEN_STR"[A-Za-z0-9 -]:\n", buffer);
	if (ret != 1) {
		*success = false;
		return NULL;
	}

	return buffer;
}

static const char * bl_msg__yaml_read_str_error(FILE *file, bool *success)
{
	int ret;

	ret = fscanf(file, "  Error: %"BUFFER_LEN_STR"[A-Za-z0-9 ]\n", buffer);
	if (ret != 1) {
		*success = false;
		return NULL;
	}

	return buffer;
}

static enum bl_msg_type bl_msg__yaml_read_type(FILE *file, bool *success)
{
	const char *str_type = bl_msg__yaml_read_str_type(file, success);

	return bl_msg_str_to_type(str_type);
}

static enum bl_error bl_msg__yaml_read_error(FILE *file, bool *success)
{
	const char *str_type = bl_msg__yaml_read_str_error(file, success);

	return bl_msg_str_to_error(str_type);
}

static uint8_t bl_msg__yaml_read_response_to(FILE *file, bool *success)
{
	unsigned value;
	int ret;

	ret = fscanf(file, "    Response to: %"BUFFER_LEN_STR"[A-Za-z0-9 ]\n",
			buffer);
	if (ret == 1) {
		return bl_msg_str_to_type(buffer);
	}

	ret = fscanf(file, "    Response to: Unknown (0x%x)\n", &value);
	if (ret == 1) {
		return value;
	}

	*success = false;
	return bl_msg_str_to_type("Unknown");
}

static uint16_t bl_msg__yaml_read_unsigned(FILE *file, const char *field, bool *success)
{
	unsigned value;
	int ret;

	ret = fscanf(file, "%"BUFFER_LEN_STR"[A-Za-z0-9 -]: %u\n",
			buffer, &value);
	if (ret == 2) {
		if (strcmp(buffer, field) == 0) {
			return value;
		}
	}

	*success = false;
	return 0;
}

static uint16_t bl_msg__yaml_read_hex(FILE *file, const char *field, bool *success)
{
	unsigned value;
	int ret;

	ret = fscanf(file, "%"BUFFER_LEN_STR"[A-Za-z0-9 -]: 0x%x\n",
			buffer, &value);
	if (ret == 2) {
		if (strcmp(buffer, field) == 0) {
			return value;
		}
	}

	*success = false;
	return 0;
}

static uint32_t bl_msg__yaml_read_unsigned_no_field(FILE *file, bool *success)
{
	uint32_t value;
	int ret;

	ret = fscanf(file, "%*[ -]%"SCNu32"\n", &value);
	if (ret == 1) {
		return value;
	}

	*success = false;
	return 0;
}

bool bl_msg_yaml_parse(FILE *file, union bl_msg_data *msg)
{
	bool ok = true;

	assert(msg != NULL);

	msg->type = bl_msg__yaml_read_type(file, &ok);

	switch (msg->type) {
	case BL_MSG_RESPONSE:
		msg->response.response_to = bl_msg__yaml_read_response_to(file, &ok);
		msg->response.error_code = bl_msg__yaml_read_error(file, &ok);
		break;

	case BL_MSG_LED:
		msg->led.led_mask = bl_msg__yaml_read_hex(file, "LED Mask", &ok);
		break;

	case BL_MSG_SOURCE_CONF:
		msg->source_conf.source        = bl_msg__yaml_read_unsigned(file, "Source",              &ok);
		msg->source_conf.opamp_gain    = bl_msg__yaml_read_unsigned(file, "Op-Amp Gain",         &ok);
		msg->source_conf.opamp_offset  = bl_msg__yaml_read_unsigned(file, "Op-Amp Offset",       &ok);
		msg->source_conf.sw_oversample = bl_msg__yaml_read_unsigned(file, "Software Oversample", &ok);
		msg->source_conf.hw_oversample = bl_msg__yaml_read_unsigned(file, "Hardware Oversample", &ok);
		msg->source_conf.hw_shift      = bl_msg__yaml_read_unsigned(file, "Hardware Shift",      &ok);
		break;

	case BL_MSG_CHANNEL_CONF:
		msg->channel_conf.channel  = bl_msg__yaml_read_unsigned(file, "Channel",  &ok);
		msg->channel_conf.source   = bl_msg__yaml_read_unsigned(file, "Source",   &ok);
		msg->channel_conf.shift    = bl_msg__yaml_read_unsigned(file, "Shift",    &ok);
		msg->channel_conf.offset   = bl_msg__yaml_read_unsigned(file, "Offset",   &ok);
		msg->channel_conf.sample32 = bl_msg__yaml_read_unsigned(file, "Sample32", &ok);
		break;

	case BL_MSG_START:
		msg->start.detection_mode = bl_msg__yaml_read_unsigned(file, "Detection Mode", &ok);
		msg->start.flash_mode     = bl_msg__yaml_read_unsigned(file, "Flash Mode",     &ok);
		msg->start.frequency      = bl_msg__yaml_read_unsigned(file, "Frequency",      &ok);
		msg->start.src_mask       = bl_msg__yaml_read_hex(file,      "Source Mask",    &ok);
		msg->start.led_mask       = bl_msg__yaml_read_hex(file,      "LED Mask",       &ok);
		break;

	case BL_MSG_SAMPLE_DATA16:
		msg->sample_data.channel = bl_msg__yaml_read_unsigned(file, "Channel", &ok);
		msg->sample_data.count   = bl_msg__yaml_read_unsigned(file, "Count",   &ok);
		ok |= (fscanf(file, "    Data:\n") == 0);
		for (unsigned i = 0; i < msg->sample_data.count; i++) {
			msg->sample_data.data16[i] = bl_msg__yaml_read_unsigned_no_field(file, &ok);
		}
		break;

	case BL_MSG_SAMPLE_DATA32:
		msg->sample_data.channel = bl_msg__yaml_read_unsigned(file, "Channel", &ok);
		msg->sample_data.count   = bl_msg__yaml_read_unsigned(file, "Count",   &ok);
		ok |= (fscanf(file, "    Data:\n") == 0);
		for (unsigned i = 0; i < msg->sample_data.count; i++) {
			msg->sample_data.data32[i] = bl_msg__yaml_read_unsigned_no_field(file, &ok);
		}
		break;

	case BL_MSG_SOURCE_CAP_REQ:
		msg->source_cap_req.source = bl_msg__yaml_read_unsigned(file, "Source", &ok);
		break;

	case BL_MSG_SOURCE_CAP:
		msg->source_cap.source         = bl_msg__yaml_read_unsigned(file, "Source",              &ok);
		msg->source_cap.hw_oversample  = bl_msg__yaml_read_unsigned(file, "Hardware Oversample", &ok);
		msg->source_cap.opamp_offset   = bl_msg__yaml_read_unsigned(file, "Op-Amp Offset",       &ok);
		msg->source_cap.opamp_gain_cnt = bl_msg__yaml_read_unsigned(file, "Op-Amp Gain Count",   &ok);
		ok |= (fscanf(file, "    Op-Amp Gains:\n") == 0);
		for (unsigned i = 0; i < msg->source_cap.opamp_gain_cnt; i++) {
			msg->source_cap.opamp_gain[i] = bl_msg__yaml_read_unsigned_no_field(file, &ok);
		}
		break;

	default:
		break;
	}

	return ok;
}

static const char *bl_msg_type_to_str(enum bl_msg_type type)
{
	if (type >= BL_ARRAY_LEN(msg_types)) {
		return NULL;
	}

	return msg_types[type];
}

static const char *bl_msg_type_to_error(enum bl_error error)
{
	if (error >= BL_ARRAY_LEN(msg_errors)) {
		return NULL;
	}

	return msg_errors[error];
}

void bl_msg_yaml_print(FILE *file, const union bl_msg_data *msg)
{
	if (bl_msg_type_to_str(msg->type) == NULL) {
		fprintf(file, "- Unknown (0x%"PRIx8")\n", msg->type);
		return;
	}

	fprintf(file, "- %s:\n", bl_msg_type_to_str(msg->type));

	switch (msg->type) {
	case BL_MSG_RESPONSE:
		if (bl_msg_type_to_str(msg->response.response_to) == NULL) {
			fprintf(file, "    Response to: Unknown (0x%"PRIx8")\n",
					msg->response.response_to);
		} else {
			fprintf(file, "    Response to: %s\n",
					bl_msg_type_to_str(
						msg->response.response_to));
		}
		fprintf(file, "    Error: %s\n",
				bl_msg_type_to_error(msg->response.error_code));
		break;

	case BL_MSG_LED:
		fprintf(file, "    LED Mask: 0x%"PRIx8"\n",
				msg->led.led_mask);
		break;

	case BL_MSG_SOURCE_CONF:
		fprintf(file, "    Source: %"PRIu8"\n",
				msg->source_conf.source);
		fprintf(file, "    Op-Amp Gain: %"PRIu8"\n",
				msg->source_conf.opamp_gain);
		fprintf(file, "    Op-Amp Offset: %"PRIu16"\n",
				msg->source_conf.opamp_offset);
		fprintf(file, "    Software Oversample: %"PRIu16"\n",
				msg->source_conf.sw_oversample);
		fprintf(file, "    Hardware Oversample: %"PRIu8"\n",
				msg->source_conf.hw_oversample);
		fprintf(file, "    Hardware Shift: %"PRIu8"\n",
				msg->source_conf.hw_shift);
		break;

	case BL_MSG_CHANNEL_CONF:
		fprintf(file, "    Channel: %"PRIu8"\n",
				msg->channel_conf.channel);
		fprintf(file, "    Source: %"PRIu8"\n",
				msg->channel_conf.source);
		fprintf(file, "    Shift: %"PRIu8"\n",
				msg->channel_conf.shift);
		fprintf(file, "    Offset: %"PRIu32"\n",
				msg->channel_conf.offset);
		fprintf(file, "    Sample32: %"PRIu8"\n",
				msg->channel_conf.sample32);
		break;

	case BL_MSG_START:
		fprintf(file, "    Detection Mode: %"PRIu8"\n",
				msg->start.detection_mode);
		fprintf(file, "    Flash Mode: %"PRIu8"\n",
				msg->start.flash_mode);
		fprintf(file, "    Frequency: %"PRIu8"\n",
				msg->start.frequency);
		fprintf(file, "    Source Mask: 0x%"PRIx16"\n",
				msg->start.src_mask);
		fprintf(file, "    LED Mask: 0x%"PRIx16"\n",
				msg->start.led_mask);
		break;

	case BL_MSG_SAMPLE_DATA16:
		fprintf(file, "    Channel: %"PRIu8"\n",
				msg->sample_data.channel);
		fprintf(file, "    Count: %"PRIu8"\n",
				msg->sample_data.count);
		fprintf(file, "    Data:\n");
		for (unsigned i = 0; i < msg->sample_data.count; i++) {
			fprintf(file, "    - %"PRIu16"\n",
					msg->sample_data.data16[i]);
		}
		break;

	case BL_MSG_SAMPLE_DATA32:
		fprintf(file, "    Channel: %"PRIu8"\n",
				msg->sample_data.channel);
		fprintf(file, "    Count: %"PRIu8"\n",
				msg->sample_data.count);
		fprintf(file, "    Data:\n");
		for (unsigned i = 0; i < msg->sample_data.count; i++) {
			fprintf(file, "    - %"PRIu32"\n",
					msg->sample_data.data32[i]);
		}
		break;

	case BL_MSG_SOURCE_CAP_REQ:
		fprintf(file, "    Source: %"PRIu8"\n",
				msg->source_cap_req.source);
		break;

	case BL_MSG_SOURCE_CAP:
		fprintf(file, "    Source: %"PRIu8"\n",
				msg->source_cap.source);
		fprintf(file, "    Hardware Oversample: %u\n",
				(unsigned) msg->source_cap.hw_oversample);
		fprintf(file, "    Op-Amp Offset: %u\n",
				(unsigned) msg->source_cap.opamp_offset);
		fprintf(file, "    Op-Amp Gain Count: %u\n",
				(unsigned) msg->source_cap.opamp_gain_cnt);
		fprintf(file, "    Op-Amp Gains:\n");
		for (unsigned i = 0; i < msg->source_cap.opamp_gain_cnt; i++) {
			fprintf(file, "    - %"PRIu8"\n",
					msg->source_cap.opamp_gain[i]);
		}
		break;

	default:
		break;
	}

	fflush(file);
}

static inline int64_t time_diff_ms(
		struct timespec *time_start,
		struct timespec *time_end)
{
	return ((time_end->tv_sec - time_start->tv_sec) * 1000 +
		(time_end->tv_nsec - time_start->tv_nsec) / 1000000);
}

static ssize_t bl_msg__read(int fd, void *data, size_t size, int timeout_ms)
{
	struct timespec time_start;
	struct timespec time_end;
	size_t total_read = 0;
	int ret;

	ret = clock_gettime(CLOCK_MONOTONIC, &time_start);
	if (ret == -1) {
		return -errno;
	}

	while (total_read != size && !bl_sig_killed) {
		ssize_t chunk_read;
		struct pollfd pfd = {
			.fd = fd,
			.events = POLLIN,
		};

		ret = poll(&pfd, 1, timeout_ms);
		if (ret == -1) {
			return -errno;

		} else if (ret == 0) {
			int elapsed_ms;

			ret = clock_gettime(CLOCK_MONOTONIC, &time_end);
			if (ret == -1) {
				return -errno;
			}

			elapsed_ms = time_diff_ms(&time_start, &time_end);
			if (elapsed_ms >= timeout_ms) {
				return -ETIMEDOUT;
			}
			timeout_ms -= elapsed_ms;
		}

		chunk_read = read(fd, (uint8_t *)data + total_read, size);
		if (chunk_read == -1) {
			return -errno;
		}

		total_read += chunk_read;
	}

	return total_read;
}

bool bl_msg_read(
		int fd,
		int timeout,
		union bl_msg_data *msg)
{
	static int previous_res;
	ssize_t expected_len;
	ssize_t read_len;
	int res = 0;

	expected_len = sizeof(msg->type);
	read_len = bl_msg__read(fd, &msg->type, expected_len, timeout);
	if (read_len != expected_len) {
		res = -read_len;
		if (res != previous_res) {
			fprintf(stderr, "Failed to read message type from device");
			if (read_len < 0)
				fprintf(stderr, ": %s", strerror(res));
			fprintf(stderr, "\n");
		}
		goto out;
	}

	expected_len = bl_msg_type_to_len(msg->type) - sizeof(msg->type);
	read_len = bl_msg__read(fd, &msg->type + sizeof(msg->type),
			expected_len, timeout);
	if (read_len != expected_len) {
		res = -read_len;
		if (res != previous_res) {
			fprintf(stderr, "Failed to read message body from device");
			if (read_len < 0)
				fprintf(stderr, ": %s", strerror(res));
			fprintf(stderr, "\n");
		}
		goto out;
	}

	if ((msg->type == BL_MSG_SAMPLE_DATA16) ||
			(msg->type == BL_MSG_SAMPLE_DATA32)) {

		size_t sample_size = msg->type == BL_MSG_SAMPLE_DATA32 ?
				sizeof(uint32_t) : sizeof(uint16_t);

		expected_len = sample_size * msg->sample_data.count;
		read_len = bl_msg__read(fd, msg->sample_data.data16,
				expected_len, timeout);

		if (read_len != expected_len) {
			res = -read_len;
			if (res != previous_res) {
				fprintf(stderr, "Failed to read %"PRIu8" samples from device",
						msg->sample_data.count);
				if (read_len < 0)
					fprintf(stderr, ": %s", strerror(res));
				fprintf(stderr, "\n");
			}
			goto out;
		}
	}

out:
	if (res == EINTR) {
		bl_sig_killed = true;
	}

	previous_res = res;

	return (res == 0);
}

bool bl_msg_write(
		int fd,
		const char *path,
		const union bl_msg_data *msg)
{
	ssize_t written;

	written = write(fd, msg, bl_msg_type_to_len(msg->type));
	if (written != bl_msg_type_to_len(msg->type)) {
		fprintf(stderr, "Failed write message to '%s'\n", path);
		return false;
	}

	return true;
}
