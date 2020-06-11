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

#define MAX_SAMPLES 32

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

/* Note: We'll break if there are more than MAX_SAMPLES in a message. */
union {
	union bl_msg_data msg;
	uint8_t samples[sizeof(union bl_msg_data) + sizeof(uint16_t) * MAX_SAMPLES];
} input;

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

