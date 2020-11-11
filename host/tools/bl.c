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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#include "common/error.h"
#include "common/util.h"
#include "common/msg.h"
#include "common/acq.h"

#include "host/common/device.h"
#include "host/common/msg.h"
#include "host/common/sig.h"

#include "util.h"

typedef int (* bl_cmd_fn)(int argc, char *argv[]);

int bl_cmd_read_and_print_message(int dev_fd, int timeout_ms)
{
	union bl_msg_data msg;

	if (bl_msg_read(dev_fd, timeout_ms, &msg)) {
		bl_msg_yaml_print(stdout, &msg);
		if (msg.type == BL_MSG_RESPONSE) {
			if (msg.response.error_code != BL_ERROR_NONE) {
				return msg.response.error_code;

			} else if (msg.response.response_to == BL_MSG_ABORT) {
				return ECONNABORTED;
			}
		}
	}

	return 0;
}

static int bl_cmd_led(int argc, char *argv[])
{
	uint32_t led_mask;
	union bl_msg_data msg = {
		.led = {
			.type = BL_MSG_LED,
		}
	};
	int ret;
	int dev_fd;
	enum {
		ARG_PROG,
		ARG_CMD,
		ARG_DEV_PATH,
		ARG_LED_MASK,
		ARG__COUNT,
	};

	if (argc != ARG__COUNT) {
		fprintf(stderr, "Usage:\n");
		fprintf(stderr, "  %s %s \\\n"
				"  \t<DEVICE_PATH|--auto|-a> \\\n"
				"  \t<LED_MASK>\n",
				argv[ARG_PROG],
				argv[ARG_CMD]);
		return EXIT_FAILURE;
	}

	if (read_sized_uint(argv[ARG_LED_MASK],
			&led_mask, sizeof(msg.led.led_mask)) == false) {
		fprintf(stderr, "Failed to parse value.\n");
		return EXIT_FAILURE;
	}

	msg.led.led_mask = led_mask;
	dev_fd = bl_device_open(argv[ARG_DEV_PATH]);
	if (dev_fd == -1) {
		return EXIT_FAILURE;
	}

	bl_msg_yaml_print(stdout, &msg);
	if (!bl_msg_write(dev_fd, argv[ARG_DEV_PATH], &msg)) {
		bl_device_close(dev_fd);
		return EXIT_FAILURE;
	}
	ret = bl_cmd_read_and_print_message(dev_fd, 10000);
	bl_device_close(dev_fd);
	return ret;

}

static int bl_cmd_channel_conf(int argc, char *argv[])
{
	union bl_msg_data msg = {
		.channel_conf = {
			.type = BL_MSG_CHANNEL_CONF,
		}
	};
	unsigned arg_optional_count;
	uint32_t sample32 = 0;
	uint32_t channel = 0;
	uint32_t source = 0;
	uint32_t offset = 0;
	uint32_t shift = 0;
	bool success;
	int ret;
	int dev_fd;
	enum {
		ARG_PROG,
		ARG_CMD,
		ARG_DEV_PATH,
		ARG_CHANNEL,
		ARG_SOURCE,
		ARG_OFFSET,
		ARG_SHIFT,
		ARG_SAMPLE32,
		ARG__COUNT,
	};

	arg_optional_count = argc - ARG_OFFSET;
	if (argc < (ARG_OFFSET) || argc > ARG__COUNT) {
		fprintf(stderr, "Usage:\n");
		fprintf(stderr, "  %s %s \\\n"
				"  \t<DEVICE_PATH|--auto|-a> \\\n"
				"  \t<CHANNEL> \\\n"
				"  \t<SOURCE> \\\n"
				"  \t[OFFSET] \\\n"
				"  \t[SHIFT] \\\n"
				"  \t[SAMPLE32]\n",
				argv[ARG_PROG],
				argv[ARG_CMD]);
		fprintf(stderr, "\n");
		fprintf(stderr, "Provide the channel specific configuration, including optional\n");
		fprintf(stderr, "shift and offset which can be used to fit values to 16-bit.\n");
		fprintf(stderr, "\n");
		fprintf(stderr, "SOURCE is the acquisition source associated with the channel.\n");
		fprintf(stderr, "\n");
		fprintf(stderr, "If an OFFSET is not provided, "
				"it will default to 0 (no offset).\n");
		fprintf(stderr, "\n");
		fprintf(stderr, "If a SHIFT value is not provided, "
				"it will default to 0 (no shift).\n");
		fprintf(stderr, "\n");
		fprintf(stderr, "If a SAMPLE32 flag is not provided, "
				"it will default to 0 (16-bit).\n");
		return EXIT_FAILURE;
	}

	success = true;
	success &= read_sized_uint(argv[ARG_CHANNEL], &channel, sizeof(msg.channel_conf.channel));
	success &= read_sized_uint(argv[ARG_SOURCE], &source, sizeof(msg.channel_conf.source));

	switch (arg_optional_count)
	{
	case 3:
		success &= read_sized_uint(argv[ARG_SAMPLE32], &sample32, sizeof(msg.channel_conf.sample32));
		/* Fall through */
	case 2:
		success &= read_sized_uint(argv[ARG_SHIFT], &shift, sizeof(msg.channel_conf.shift));
		/* Fall through */
	case 1:
		success &= read_sized_uint(argv[ARG_OFFSET], &offset, sizeof(msg.channel_conf.offset));
		break;
	}

	if (!success) {
		fprintf(stderr, "Failed to parse arguments.\n");
		return EXIT_FAILURE;
	}

	msg.channel_conf.sample32 = sample32;
	msg.channel_conf.channel = channel;
	msg.channel_conf.source = source;
	msg.channel_conf.offset = offset;
	msg.channel_conf.shift = shift;

	dev_fd = bl_device_open(argv[ARG_DEV_PATH]);
	if (dev_fd == -1) {
		return EXIT_FAILURE;
	}

	bl_msg_yaml_print(stdout, &msg);
	if (!bl_msg_write(dev_fd, argv[ARG_DEV_PATH], &msg)) {
		bl_device_close(dev_fd);
		return EXIT_FAILURE;
	}
	ret = bl_cmd_read_and_print_message(dev_fd, 10000);
	bl_device_close(dev_fd);
	return ret;

}

static int bl_cmd_source_conf(int argc, char *argv[])
{
	union bl_msg_data msg = {
		.source_conf = {
			.type = BL_MSG_SOURCE_CONF,
		}
	};
	unsigned arg_optional_count;
	uint32_t hw_oversample = 0;
	uint32_t sw_oversample = 0;
	uint32_t opamp_offset = 0;
	uint32_t opamp_gain = 0;
	uint32_t hw_shift = 0;
	uint32_t source = 0;
	bool success;
	int dev_fd;
	int ret;
	enum {
		ARG_PROG,
		ARG_CMD,
		ARG_DEV_PATH,
		ARG_SOURCE,
		ARG_OPAMP_GAIN,
		ARG_OPAMP_OFFSET,
		ARG_SOFTWARE_OVERSAMPLE,
		ARG_HARDWARE_OVERSAMPLE,
		ARG_HARDWARE_SHIFT,
		ARG__COUNT,
	};

	arg_optional_count = argc - ARG_HARDWARE_OVERSAMPLE;
	if (argc < (ARG_HARDWARE_OVERSAMPLE) || argc > ARG__COUNT) {
		fprintf(stderr, "Usage:\n");
		fprintf(stderr, "  %s %s \\\n"
				"  \t<DEVICE_PATH|--auto|-a> \\\n"
				"  \t<SOURCE> \\\n"
				"  \t<OPAMP GAIN> \\\n"
				"  \t<OPAMP OFFSET>\\\n"
				"  \t<SOFTWARE OVERSAMPLE> \\\n"
				"  \t[HARDWARE OVERSAMPLE] \\\n"
				"  \t[HARDWARE SHIFT]\n",
				argv[ARG_PROG],
				argv[ARG_CMD]);
		fprintf(stderr, "\n");
		fprintf(stderr, "Provide the source specific configuration\n");
		return EXIT_FAILURE;
	}

	success = true;
	success &= read_sized_uint(argv[ARG_SOURCE],
			&source,
			sizeof(msg.source_conf.source));
	success &= read_sized_uint(argv[ARG_OPAMP_GAIN],
			&opamp_gain,
			sizeof(msg.source_conf.opamp_gain));
	success &= read_sized_uint(argv[ARG_OPAMP_OFFSET],
			&opamp_offset,
			sizeof(msg.source_conf.opamp_offset));
	success &= read_sized_uint(argv[ARG_SOFTWARE_OVERSAMPLE],
			&sw_oversample,
			sizeof(msg.source_conf.sw_oversample));

	switch (arg_optional_count)
	{
	case 2:
		success &= read_sized_uint(argv[ARG_HARDWARE_OVERSAMPLE],
				&hw_oversample,
				sizeof(msg.source_conf.hw_oversample));
		/* Fall through */
	case 1:
		success &= read_sized_uint(argv[ARG_HARDWARE_SHIFT],
				&hw_shift,
				sizeof(msg.source_conf.hw_shift));
		break;
	}

	if (!success) {
		fprintf(stderr, "Failed to parse arguments.\n");
		return EXIT_FAILURE;
	}

	msg.source_conf.source = source;
	msg.source_conf.opamp_gain = opamp_gain;
	msg.source_conf.opamp_offset = opamp_offset;
	msg.source_conf.sw_oversample = sw_oversample;
	msg.source_conf.hw_oversample = hw_oversample;
	msg.source_conf.hw_shift = hw_shift;

	dev_fd = bl_device_open(argv[ARG_DEV_PATH]);
	if (dev_fd == -1) {
		return EXIT_FAILURE;
	}

	bl_msg_yaml_print(stdout, &msg);
	if (!bl_msg_write(dev_fd, argv[ARG_DEV_PATH], &msg)) {
		bl_device_close(dev_fd);
		return EXIT_FAILURE;
	}
	ret = bl_cmd_read_and_print_message(dev_fd, 10000);
	bl_device_close(dev_fd);
	return ret;

}

static int bl_cmd_source_cap(int argc, char *argv[])
{
	union bl_msg_data msg = {
		.source_conf = {
			.type = BL_MSG_SOURCE_CAP_REQ,
		}
	};
	uint32_t source = 0;
	bool success;
	int dev_fd;
	int ret;
	enum {
		ARG_PROG,
		ARG_CMD,
		ARG_DEV_PATH,
		ARG_SOURCE,
		ARG__COUNT,
	};

	if (argc != ARG__COUNT) {
		fprintf(stderr, "Usage:\n");
		fprintf(stderr, "  %s %s \\\n"
				"  \t<DEVICE_PATH|--auto|-a> \\\n"
				"  \t<SOURCE>\n",
				argv[ARG_PROG],
				argv[ARG_CMD]);
		fprintf(stderr, "\n");
		fprintf(stderr, "Get source capabilities\n");
		return EXIT_FAILURE;
	}

	success = read_sized_uint(argv[ARG_SOURCE],
			&source,
			sizeof(msg.source_conf.source));
	if (!success) {
		fprintf(stderr, "Failed to parse source parameter.\n");
		return EXIT_FAILURE;
	}

	msg.source_cap_req.source = source;

	dev_fd = bl_device_open(argv[ARG_DEV_PATH]);
	if (dev_fd == -1) {
		return EXIT_FAILURE;
	}

	bl_msg_yaml_print(stdout, &msg);
	if (!bl_msg_write(dev_fd, argv[ARG_DEV_PATH], &msg)) {
		bl_device_close(dev_fd);
		return EXIT_FAILURE;
	}
	ret = bl_cmd_read_and_print_message(dev_fd, 10000);
	bl_device_close(dev_fd);
	return ret;
}

static int bl_cmd__no_params_helper(
		int argc,
		char *argv[],
		enum bl_msg_type type)
{
	union bl_msg_data msg = {
		.type = type,
	};
	int ret;
	int dev_fd;
	enum {
		ARG_PROG,
		ARG_CMD,
		ARG_DEV_PATH,
		ARG__COUNT,
	};

	if (argc != ARG__COUNT) {
		fprintf(stderr, "Usage:\n");
		fprintf(stderr, "  %s %s <DEVICE_PATH|--auto|-a>\n",
				argv[ARG_PROG], argv[ARG_CMD]);
		return EXIT_FAILURE;
	}

	dev_fd = bl_device_open(argv[ARG_DEV_PATH]);
	if (dev_fd == -1) {
		return EXIT_FAILURE;
	}
	bl_msg_yaml_print(stdout, &msg);
	if (!bl_msg_write(dev_fd, argv[ARG_DEV_PATH], &msg)) {
		bl_device_close(dev_fd);
		return EXIT_FAILURE;
	}
	ret = bl_cmd_read_and_print_message(dev_fd, 10000);
	bl_device_close(dev_fd);
	return ret;
}

int bl_cmd_receive_and_print_loop(int dev_fd)
{
	int ret = EXIT_SUCCESS;
	do {
		ret = bl_cmd_read_and_print_message(dev_fd, 10000);
		if (ret == ECONNABORTED) {
			return EXIT_SUCCESS;
		} else if (ret != 0) {
			ret = EXIT_FAILURE;
		}
	} while (!bl_sig_killed);
	return ret;
}

static int bl_cmd_start_stream(
		int argc,
		char *argv[])
{
	union bl_msg_data msg = {
		.start = {
			.type = BL_MSG_START,
		}
	};
	uint32_t src_mask, led_mask;
	uint32_t frequency;
	int ret;
	int dev_fd;
	enum {
		ARG_PROG,
		ARG_CMD,
		ARG_DEV_PATH,
		ARG_FLASH_MODE,
		ARG_DETECT_MODE,
		ARG_FREQUENCY,
		ARG_SRC_MASK,
		ARG_LED_MASK,
		ARG__COUNT,
	};

	if (argc != ARG__COUNT) {
		fprintf(stderr, "Usage:\n");
		fprintf(stderr, "  %s %s \\\n"
				"  \t<DEVICE_PATH|--auto|-a> \\\n"
				"  \t<--flash|-f|--continous|-c> \\\n"
				"  \t<--transmissive|-t|--reflective|-r> \\\n"
				"  \t<FREQUENCY> \\\n"
				"  \t<SRC_MASK>\\\n"
				"  \t<LED_MASK>\n",
				argv[ARG_PROG],
				argv[ARG_CMD]);
		fprintf(stderr, "\n");
		fprintf(stderr, "FREQUENCY is the sampling rate in Hz.\n");
		return EXIT_FAILURE;
	}

	if (read_sized_uint(argv[ARG_SRC_MASK],
			&src_mask, sizeof(msg.start.src_mask)) == false ||
		read_sized_uint(argv[ARG_LED_MASK],
		    &led_mask, sizeof(msg.start.led_mask)) == false ||
		read_sized_uint(argv[ARG_FREQUENCY],
			&frequency, sizeof(msg.start.frequency)) == false) {
		fprintf(stderr, "Failed to parse value.\n");
		return EXIT_FAILURE;
	}

	if (!strncmp(argv[ARG_FLASH_MODE], "--flash", 8) ||
		  !strncmp(argv[ARG_FLASH_MODE], "-f", 3)) {
		msg.start.flash_mode  = BL_ACQ_FLASH;
	} else {
		msg.start.flash_mode  = BL_ACQ_CONTINUOUS;
	}
	if (!strncmp(argv[ARG_DETECT_MODE], "--transmissive", 15) ||
		  !strncmp(argv[ARG_FLASH_MODE], "-t", 3)) {
		msg.start.detection_mode  = BL_ACQ_TRANSMISSIVE;
	} else {
		msg.start.detection_mode  = BL_ACQ_REFLECTIVE;
	}


	msg.start.frequency  = frequency;
	msg.start.src_mask   = src_mask;
	msg.start.led_mask   = led_mask;

	dev_fd = bl_device_open(argv[ARG_DEV_PATH]);
	if (dev_fd == -1) {
		return EXIT_FAILURE;
	}

	bl_msg_yaml_print(stdout, &msg);

	if (!bl_msg_write(dev_fd, argv[ARG_DEV_PATH], &msg)) {
		bl_device_close(dev_fd);
		return EXIT_FAILURE;
	}

	ret = bl_cmd_receive_and_print_loop(dev_fd);

	/* Send abort after ctrl+c */
	if (bl_sig_killed) {
		union bl_msg_data abort_msg = {
			.type = BL_MSG_ABORT,
		};
		bl_msg_write(dev_fd, argv[ARG_DEV_PATH], &abort_msg);
		bl_sig_killed = false;
		bl_cmd_receive_and_print_loop(dev_fd);
	}

	bl_device_close(dev_fd);
	return ret;

}

int bl_cmd_start(int argc, char *argv[])
{
	return bl_cmd_start_stream(argc, argv);
}

static int bl_cmd_abort(int argc, char *argv[])
{
	return bl_cmd__no_params_helper(argc, argv, BL_MSG_ABORT);
}

static const struct bl_cmd {
	const char *name;
	const char *help;
	const bl_cmd_fn fn;
} cmds[] = {
	{
		.name = "led",
		.help = "Turn LEDs on/off",
		.fn = bl_cmd_led,
	},
	{
		.name = "srccap",
		.help = "Get source capabilities",
		.fn = bl_cmd_source_cap
	},
	{
		.name = "srccfg",
		.help = "Set configuration for a given source",
		.fn = bl_cmd_source_conf
	},
	{
		.name = "chancfg",
		.help = "Set configuration for a given channel",
		.fn = bl_cmd_channel_conf
	},
	{
		.name = "start",
		.help = "Start an acquisition",
		.fn = bl_cmd_start,
	},
	{
		.name = "abort",
		.help = "Abort an acquisition",
		.fn = bl_cmd_abort,
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

void get_dev(int dev, char *argv[])
{
	/* Auto device lookup is handled with a NULL path to bl_device_open() */
	if (strcmp(argv[dev], "--auto") ||
	    strcmp(argv[dev], "-a")) {
		argv[dev] = NULL;
	}
}

int main(int argc, char *argv[])
{
	bl_cmd_fn cmd_fn;
	enum {
		ARG_PROG,
		ARG_CMD,
		ARG_DEV_PATH,
		ARG__COUNT,
	};

	if (argc < ARG_DEV_PATH) {
		bl_cmd_help(argv[ARG_PROG]);
		return EXIT_FAILURE;
	}

	/* All valid comments using device path have more than 2 arguments */
	if (argc > ARG_DEV_PATH)
		get_dev(ARG_DEV_PATH, argv);

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
