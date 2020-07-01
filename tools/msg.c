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
	[BL_MSG_RESPONSE]        = "Response",
	[BL_MSG_LED]             = "LED",
	[BL_MSG_START]           = "Start",
	[BL_MSG_ABORT]           = "Abort",
	[BL_MSG_SAMPLE_DATA]     = "Sample Data",
	[BL_MSG_SET_GAINS]       = "Set Gains",
	[BL_MSG_SET_OVERSAMPLE]  = "Set Oversample",
	[BL_MSG_SET_FIXEDOFFSET] = "Set Fixed Offset",
};

static inline enum bl_msg_type bl_msg_str_to_type(const char *str)
{
	if (str != NULL) {
		for (unsigned i = 0; i < BL_ARRAY_LEN(msg_types); i++) {
			if (msg_types[i] != NULL) {
				if (strcmp(msg_types[i], str) == 0) {
					return i;
				}
			}
		}
	}

	return BL_MSG__COUNT;
}

static const char * bl_msg__read_str_type(bool *success)
{
	int ret;

	ret = scanf("- %"BUFFER_LEN_STR"[A-Za-z0-9 ]:\n", buffer);
	if (ret != 1) {
		*success = false;
		return NULL;
	}

	return buffer;
}

static enum bl_msg_type bl_msg__read_type(bool *success)
{
	const char *str_type = bl_msg__read_str_type(success);

	return bl_msg_str_to_type(str_type);
}

static uint8_t bl_msg__read_response_to(bool *success)
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

static uint16_t bl_msg__read_unsigned(bool *success, const char *field)
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

static uint16_t bl_msg__read_hex(bool *success, const char *field)
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

static uint16_t bl_msg__read_unsigned_no_field(bool *success)
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

bool bl_msg_parse(union bl_msg_data *msg)
{
	bool ok = true;

	assert(msg != NULL);

	msg->type = bl_msg__read_type(&ok);

	switch (msg->type) {
	case BL_MSG_RESPONSE:
		msg->response.response_to = bl_msg__read_response_to(&ok);
		msg->response.error_code = bl_msg__read_unsigned(&ok, "Error code");
		break;

	case BL_MSG_LED:
		msg->led.led_mask = bl_msg__read_hex(&ok, "LED Mask");
		break;

	case BL_MSG_START:
		msg->start.frequency = bl_msg__read_unsigned(&ok, "Frequency");
		msg->start.src_mask = bl_msg__read_hex(&ok, "Source Mask");
		break;

	case BL_MSG_SAMPLE_DATA:
		msg->sample_data.count = bl_msg__read_unsigned(&ok, "Count");
		msg->sample_data.src_mask = bl_msg__read_hex(&ok, "Source Mask");
		ok |= (scanf("    Data:\n") == 0);
		for (unsigned i = 0; i < msg->sample_data.count; i++) {
			msg->sample_data.data[i] = bl_msg__read_unsigned_no_field(&ok);
		}
		break;

	case BL_MSG_SET_GAINS:
		ok |= (scanf("    Gains:\n") == 0);
		for (unsigned i = 0; i < BL_ACQ_PD__COUNT; i++) {
			msg->gain.gain[i] = bl_msg__read_unsigned_no_field(&ok);
		}
		break;

	case BL_MSG_SET_OVERSAMPLE:
		msg->oversample.oversample = bl_msg__read_unsigned(&ok, "Oversample");
		break;

	case BL_MSG_SET_FIXEDOFFSET:
		ok |= (scanf("    Offset:\n") == 0);
		for (unsigned i = 0; i < BL_ACQ__SRC_COUNT; i++) {
			msg->gain.gain[i] = bl_msg__read_unsigned_no_field(&ok);
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

void bl_msg_print(const union bl_msg_data *msg, FILE *file)
{
	if (bl_msg_type_to_str(msg->type) == NULL) {
		fprintf(file, "- Unknown (0x%x)\n",
				(unsigned) msg->type);
		return;
	}

	fprintf(file, "- %s:\n", bl_msg_type_to_str(msg->type));

	switch (msg->type) {
	case BL_MSG_RESPONSE:
		if (bl_msg_type_to_str(msg->response.response_to) == NULL) {
			fprintf(file, "    Response to: Unknown (0x%x)\n",
					(unsigned) msg->response.response_to);
		} else {
			fprintf(file, "    Response to: %s\n",
					bl_msg_type_to_str(
						msg->response.response_to));
		}
		fprintf(file, "    Error code: %u\n",
				(unsigned) msg->response.error_code);
		break;

	case BL_MSG_LED:
		fprintf(file, "    LED Mask: 0x%x\n",
				(unsigned) msg->led.led_mask);
		break;

	case BL_MSG_START:
		fprintf(file, "    Frequency: %u\n",
				(unsigned) msg->start.frequency);
		fprintf(file, "    Source Mask: 0x%x\n",
				(unsigned) msg->start.src_mask);
		break;

	case BL_MSG_SAMPLE_DATA:
		fprintf(file, "    Count: %u\n",
				(unsigned) msg->sample_data.count);
		fprintf(file, "    Source Mask: 0x%x\n",
				(unsigned) msg->sample_data.src_mask);
		fprintf(file, "    Data:\n");
		for (unsigned i = 0; i < msg->sample_data.count; i++) {
			fprintf(file, "    - %u\n",
				(unsigned) msg->sample_data.data[i]);
		}
		break;

	case BL_MSG_SET_GAINS:
		fprintf(file, "    Gains:\n");
		for (unsigned i = 0; i < BL_ACQ_PD__COUNT; i++) {
			fprintf(file, "    - %u\n",
				(unsigned) msg->gain.gain[i]);
		}
		break;

	case BL_MSG_SET_OVERSAMPLE:
		fprintf(file, "    Oversample: %u\n", msg->oversample.oversample);
		break;

	case BL_MSG_SET_FIXEDOFFSET:
		fprintf(file, "    Offset:\n");
		for (unsigned i = 0; i < BL_ACQ__SRC_COUNT; i++) {
			fprintf(file, "    - %u\n",
				(unsigned) msg->offset.offset[i]);
		}
		break;

	default:
		break;
	}

	fflush(file);
}
