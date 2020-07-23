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
#include <stdio.h>

#include "../src/error.h"
#include "../src/util.h"
#include "../src/msg.h"

#include "msg.h"

#define BUFFER_LEN 64
#define BUFFER_LEN_STR "64"

static char buffer[BUFFER_LEN + 1];

/** Message type to string mapping, */
static const char *msg_types[] = {
	[BL_MSG_RESPONSE]      = "Response",
	[BL_MSG_LED]           = "LED",
	[BL_MSG_CHANNEL_CONF]  = "Channel Config",
	[BL_MSG_START]         = "Start",
	[BL_MSG_ABORT]         = "Abort",
	[BL_MSG_SAMPLE_DATA16] = "Sample Data 16-bit",
	[BL_MSG_SAMPLE_DATA32] = "Sample Data 32-bit",
};

/** Message type to string mapping, */
static const char *msg_errors[] = {
	[BL_ERROR_NONE]               = "Success",
	[BL_ERROR_OUT_OF_RANGE]       = "Value out of range",
	[BL_ERROR_BAD_MESSAGE_TYPE]   = "Bad message type",
	[BL_ERROR_BAD_MESSAGE_LENGTH] = "Bad message length",
	[BL_ERROR_BAD_SOURCE_MASK]    = "Bad source mask",
	[BL_ERROR_ACTIVE_ACQUISITION] = "In acquisition state",
	[BL_ERROR_BAD_FREQUENCY]      = "Unsupported frequency combination",
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

	ret = fscanf(file, "%"BUFFER_LEN_STR"[A-Za-z0-9 ]: %u\n",
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

	ret = fscanf(file, "%"BUFFER_LEN_STR"[A-Za-z0-9 ]: 0x%x\n",
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

	case BL_MSG_CHANNEL_CONF:
		msg->channel_conf.channel  = bl_msg__yaml_read_unsigned(file, "Channel",  &ok);
		msg->channel_conf.gain     = bl_msg__yaml_read_unsigned(file, "Gain",     &ok);
		msg->channel_conf.shift    = bl_msg__yaml_read_unsigned(file, "Shift",    &ok);
		msg->channel_conf.offset   = bl_msg__yaml_read_unsigned(file, "Offset",   &ok);
		msg->channel_conf.sample32 = bl_msg__yaml_read_unsigned(file, "Sample32", &ok);
		break;

	case BL_MSG_START:
		msg->start.frequency  = bl_msg__yaml_read_unsigned(file, "Frequency",  &ok);
		msg->start.oversample = bl_msg__yaml_read_unsigned(file, "Oversample", &ok);
		msg->start.src_mask   = bl_msg__yaml_read_hex(file, "Source Mask",     &ok);
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

	case BL_MSG_CHANNEL_CONF:
		fprintf(file, "    Channel: %"PRIu8"\n",
				msg->channel_conf.channel);
		fprintf(file, "    Gain: %"PRIu8"\n",
				msg->channel_conf.gain);
		fprintf(file, "    Shift: %"PRIu8"\n",
				msg->channel_conf.shift);
		fprintf(file, "    Offset: %"PRIu32"\n",
				msg->channel_conf.offset);
		fprintf(file, "    Sample32: %"PRIu8"\n",
				msg->channel_conf.sample32);
		break;

	case BL_MSG_START:
		fprintf(file, "    Frequency: %"PRIu8"\n",
				msg->start.frequency);
		fprintf(file, "    Oversample: %"PRIu8"\n",
				msg->start.oversample);
		fprintf(file, "    Source Mask: 0x%"PRIx8"\n",
				msg->start.src_mask);
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

	default:
		break;
	}

	fflush(file);
}
