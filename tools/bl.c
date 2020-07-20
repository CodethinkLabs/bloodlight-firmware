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
#include <errno.h>
#include <stdio.h>
#include <time.h>

#include <termios.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include "../src/error.h"
#include "../src/util.h"
#include "../src/msg.h"

/* Common helper functionality. */
#include "msg.h"
#include "find_device.h"

/* Whether we've had a `ctrl-c`. */
volatile bool killed;

typedef int (* bl_cmd_fn)(int argc, char *argv[]);

static inline bool check_fit(uint32_t value, size_t target_size)
{
	uint32_t target_max;

	assert(target_size <= 4);

	target_max = (~(uint32_t)0) >> ((sizeof(uint32_t) - target_size) * 8);

	if (value > target_max) {
		return false;
	}

	return true;
}

static inline bool read_sized_uint(
		const char *value,
		uint32_t *out,
		size_t target_size)
{
	unsigned long long temp;
	char *end = NULL;

	errno = 0;
	temp = strtoull(value, &end, 0);

	if (end == value || errno == ERANGE || temp > UINT32_MAX) {
		return false;
	}

	if (!check_fit(temp, target_size)) {
		return false;
	}

	*out = (uint32_t)temp;
	return true;
}



static inline int64_t time_diff_ms(
		struct timespec *time_start,
		struct timespec *time_end)
{
	return ((time_end->tv_sec - time_start->tv_sec) * 1000 +
		(time_end->tv_nsec - time_start->tv_nsec) / 1000000);
}

ssize_t bl_read(int fd, void *data, size_t size, int timeout_ms)
{
	struct timespec time_start;
	struct timespec time_end;
	size_t total_read = 0;
	int ret;

	ret = clock_gettime(CLOCK_MONOTONIC, &time_start);
	if (ret == -1) {
		fprintf(stderr, "Failed to read time: %s\n",
				strerror(errno));
		return -errno;
	}

	while (total_read != size && !killed) {
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
				fprintf(stderr, "Failed to read time: %s\n",
						strerror(errno));
				return -errno;
			}

			elapsed_ms = time_diff_ms(&time_start, &time_end);
			if (elapsed_ms >= timeout_ms) {
				fprintf(stderr, "Timed out waiting for read\n");
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

static uint8_t bl_response_data[64];

static int bl_cmd_read_message(
		int dev_fd,
		int timeout_ms,
		union bl_msg_data ** response)
{
	union bl_msg_data * const msg = (void *) &bl_response_data[0];
	(*response) = msg;
	ssize_t expected_len;
	ssize_t read_len;

	expected_len = sizeof(msg->type);
	read_len = bl_read(dev_fd, &msg->type, expected_len, timeout_ms);
	if (read_len != expected_len) {
		fprintf(stderr, "Failed to read message type from device");
		if (read_len < 0)
			fprintf(stderr, ": %s", strerror(-read_len));
		fprintf(stderr, "\n");
		return -read_len;
	}

	expected_len = bl_msg_type_to_len(msg->type) - sizeof(msg->type);
	read_len = bl_read(dev_fd, &msg->type + sizeof(msg->type),
			expected_len, timeout_ms);
	if (read_len != expected_len) {
		fprintf(stderr, "Failed to read message body from device");
		if (read_len < 0)
			fprintf(stderr, ": %s", strerror(-read_len));
		fprintf(stderr, "\n");
		return -read_len;
	}

	if (msg->type == BL_MSG_SAMPLE_DATA) {
		expected_len = sizeof(uint16_t) * msg->sample_data.count;
		read_len = bl_read(dev_fd, &msg->sample_data.data[0],
				expected_len, timeout_ms);
		if (read_len != expected_len) {
			fprintf(stderr, "Failed to read %u samples from device",
					(unsigned) msg->sample_data.count);
			if (read_len < 0)
				fprintf(stderr, ": %s", strerror(-read_len));
			fprintf(stderr, "\n");
			return -read_len;
		}
	}

	return 0;
}

int bl_cmd_read_and_print_message(int dev_fd, int timeout_ms)
{
	union bl_msg_data * msg = NULL;
	int ret = bl_cmd_read_message(dev_fd, timeout_ms, &msg);
	if (ret == 0) {
		bl_msg_print(msg, stdout);
		switch (msg->type) {
		case BL_MSG_RESPONSE:
			if (msg->response.error_code != BL_ERROR_NONE) {
				return msg->response.error_code;
			}
			break;
		case BL_MSG_ABORTED:
			return -ECONNABORTED;
		}
	}

	return ret;
}

static int bl_open_device(const char *dev_path)
{
	int dev_fd = open(dev_path, O_RDWR);
	if (dev_fd == -1) {
		fprintf(stderr, "Failed to open '%s': %s\n",
				dev_path, strerror(errno));
		return -1;
	}

	struct termios t;
	if (tcgetattr(dev_fd, &t) == 0) {
		t.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
		t.c_oflag &= ~(OPOST);
		t.c_cflag |=  (CS8);
		t.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

		if (tcsetattr(dev_fd, TCSANOW, &t) != 0) {
			fprintf(stderr, "Failed set terminal attributes"
				" of '%s': %s\n", dev_path, strerror(errno));
			return -1;
		}
	} else {
		fprintf(stderr, "Failed get terminal attributes"
				" of '%s': %s\n", dev_path, strerror(errno));
		return -1;
	}
	return dev_fd;
}

static void bl_close_device(int dev_fd)
{
	if(dev_fd >= 0) {/* Maybe we should ignore 0, 1 and 2 */
		close(dev_fd);
	}
}

static int bl_cmd_send(
		const union bl_msg_data *msg,
		const char *dev_path, int dev_fd)
{
	ssize_t written;
	int ret = EXIT_SUCCESS;

	written = write(dev_fd, msg, bl_msg_type_to_len(msg->type));
	if (written != bl_msg_type_to_len(msg->type)) {
		fprintf(stderr, "Failed write message to '%s'\n", dev_path);
		ret = EXIT_FAILURE;
	}

	return ret;
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
				"  \t<DEVICE_PATH|auto|--auto|-a> \\\n"
				"  \t<LED_MASK>\n",
				argv[ARG_PROG],
				argv[ARG_CMD]);
		return EXIT_FAILURE;
	}

	if (read_sized_uint(argv[ARG_LED_MASK],
	                    &led_mask,
						sizeof(msg.led.led_mask)) == false) {
		fprintf(stderr, "Failed to parse value.\n");
		return EXIT_FAILURE;
	}

	msg.led.led_mask = led_mask;
	dev_fd = bl_open_device(argv[ARG_DEV_PATH]);
	if (dev_fd == -1) {
		return EXIT_FAILURE;
	}

	bl_msg_print(&msg, stdout);
	ret = bl_cmd_send(&msg, argv[ARG_DEV_PATH], dev_fd);
	if (ret != EXIT_SUCCESS) {
		bl_close_device(dev_fd);
		return ret;
	}
	ret = bl_cmd_read_and_print_message(dev_fd, 10000);
	bl_close_device(dev_fd);
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
	uint32_t saturate;
	uint32_t channel;
	uint32_t offset;
	uint32_t shift;
	uint32_t gain;
	int ret;
	int dev_fd;
	enum {
		ARG_PROG,
		ARG_CMD,
		ARG_DEV_PATH,
		ARG_CHANNEL,
		ARG_GAIN,
		ARG_OFFSET,
		ARG_SHIFT,
		ARG_SATURATE,
		ARG__COUNT,
	};

	arg_optional_count = argc - ARG_OFFSET;
	if (argc < (ARG_OFFSET) || argc > ARG__COUNT) {
		fprintf(stderr, "Usage:\n");
		fprintf(stderr, "  %s %s \\\n"
				"  \t<DEVICE_PATH|auto|--auto|-a> \\\n"
				"  \t<CHANNEL> \\\n"
				"  \t<GAIN> \\\n"
				"  \t[OFFSET] \\\n"
				"  \t[SHIFT] \\\n"
				"  \t[SATURATE]\n",
				argv[ARG_PROG],
				argv[ARG_CMD]);
		fprintf(stderr, "\n");
		fprintf(stderr, "A GAIN value must be a power of two"
				" up to 16.\n");
		fprintf(stderr, "\n");
		fprintf(stderr, "Provide the amount to subtract from accumulated values in-chip,\n");
		fprintf(stderr, "optional shift and saturate can be used to fit values to 16-bit.\n");
		fprintf(stderr, "\n");
		fprintf(stderr, "If an OFFSET is not provided, "
				"it will default to 0 (no offset).\n");
		fprintf(stderr, "\n");
		fprintf(stderr, "If a SHIFT value is not provided, "
				"it will default to 0 (no shift).\n");
		fprintf(stderr, "\n");
		fprintf(stderr, "If a SATURATE flag is not provided, "
				"it will default to 1 (saturation enabled).\n");
		return EXIT_FAILURE;
	}

	if (!read_sized_uint(argv[ARG_CHANNEL], &channel, sizeof(msg.channel_conf.channel))) {
		fprintf(stderr, "Failed to parse value.\n");
		return EXIT_FAILURE;
	}
	msg.channel_conf.channel = channel;

	if (!read_sized_uint(argv[ARG_GAIN], &gain, sizeof(msg.channel_conf.gain))) {
		fprintf(stderr, "Failed to parse value.\n");
		return EXIT_FAILURE;
	}
	msg.channel_conf.gain = gain;

	switch (arg_optional_count)
	{
	case 3:
		if (!read_sized_uint(argv[ARG_SATURATE], &saturate, sizeof(msg.channel_conf.saturate))) {
			fprintf(stderr, "Failed to parse saturate value.\n");
			return EXIT_FAILURE;
		}
		msg.channel_conf.saturate = saturate;
		/* Fall through */

	case 2:
		if (!read_sized_uint(argv[ARG_SHIFT], &shift, sizeof(msg.channel_conf.shift))) {
			fprintf(stderr, "Failed to parse shift value.\n");
			return EXIT_FAILURE;
		}
		msg.channel_conf.shift = shift;
		/* Fall through */

	case 1:
		if (!read_sized_uint(argv[ARG_OFFSET], &offset, sizeof(msg.channel_conf.offset))) {
			fprintf(stderr, "Failed to parse offset value.\n");
			return EXIT_FAILURE;
		}
		msg.channel_conf.offset = offset;
		break;
	}

	dev_fd = bl_open_device(argv[ARG_DEV_PATH]);
	if (dev_fd == -1) {
		return EXIT_FAILURE;
	}

	bl_msg_print(&msg, stdout);
	ret = bl_cmd_send(&msg, argv[ARG_DEV_PATH], dev_fd);
	if (ret != EXIT_SUCCESS) {
		bl_close_device(dev_fd);
		return ret;
	}
	ret = bl_cmd_read_and_print_message(dev_fd, 10000);
	bl_close_device(dev_fd);
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
		fprintf(stderr, "  %s %s <DEVICE_PATH|auto|--auto|-a>\n",
				argv[ARG_PROG], argv[ARG_CMD]);
		return EXIT_FAILURE;
	}

	dev_fd = bl_open_device(argv[ARG_DEV_PATH]);
	if (dev_fd == -1) {
		return EXIT_FAILURE;
	}
	bl_msg_print(&msg, stdout);
	ret = bl_cmd_send(&msg, argv[ARG_DEV_PATH], dev_fd);
	if (ret != EXIT_SUCCESS) {
		bl_close_device(dev_fd);
		return ret;
	}
	ret = bl_cmd_read_and_print_message(dev_fd, 10000);
	bl_close_device(dev_fd);
	return ret;
}

int bl_cmd_receive_and_print_loop(int dev_fd)
{
	int ret = EXIT_SUCCESS;
	do {
		ret = bl_cmd_read_and_print_message(dev_fd, 10000);
		if (ret == EINTR) {
			killed = true;

		} else if (ret != 0) {
			ret = EXIT_FAILURE;
		}
	} while (!killed);
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
	uint32_t src_mask;
	uint32_t frequency, oversample;
	int ret;
	int dev_fd;
	enum {
		ARG_PROG,
		ARG_CMD,
		ARG_DEV_PATH,
		ARG_FREQUENCY,
		ARG_OVERSAMPLE,
		ARG_SRC_MASK,
		ARG__COUNT,
	};

	if (argc != ARG__COUNT) {
		fprintf(stderr, "Usage:\n");
		fprintf(stderr, "  %s %s \\\n"
				"  \t<DEVICE_PATH|auto|--auto|-a> \\\n"
				"  \t<FREQUENCY> \\\n"
				"  \t<OVERSAMPLE> \\\n"
				"  \t<SRC_MASK>\n",
				argv[ARG_PROG],
				argv[ARG_CMD]);
		fprintf(stderr, "\n");
		fprintf(stderr, "FREQUENCY is the sampling rate in Hz.\n");
		fprintf(stderr, "\n");
		fprintf(stderr, "OVERSAMPLE is the number of samples to sum.\n");
		return EXIT_FAILURE;
	}

	if (read_sized_uint(argv[ARG_SRC_MASK],
			&src_mask, sizeof(msg.start.src_mask)) == false ||
	    read_sized_uint(argv[ARG_FREQUENCY],
			&frequency, sizeof(msg.start.frequency)) == false ||
	    read_sized_uint(argv[ARG_OVERSAMPLE],
			&oversample, sizeof(msg.start.oversample)) == false ) {
		fprintf(stderr, "Failed to parse value.\n");
		return EXIT_FAILURE;
	}

	msg.start.frequency  = frequency;
	msg.start.oversample = oversample;
	msg.start.src_mask   = src_mask;

	dev_fd = bl_open_device(argv[ARG_DEV_PATH]);
	if (dev_fd == -1) {
		return EXIT_FAILURE;
	}

	bl_msg_print(&msg, stdout);

	ret = bl_cmd_send(&msg, argv[ARG_DEV_PATH], dev_fd);

	if (ret != BL_ERROR_NONE) {
		bl_close_device(dev_fd);
		return EXIT_FAILURE;
	}

	ret = bl_cmd_receive_and_print_loop(dev_fd);

	/* Send abort after ctrl+c */
	if (killed) {
		union bl_msg_data abort_msg = {
			.type = BL_MSG_ABORT,
		};
		bl_cmd_send(&abort_msg, argv[ARG_DEV_PATH],dev_fd);
		killed = false;
		bl_cmd_receive_and_print_loop(dev_fd);
	}

	bl_close_device(dev_fd);
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

static void bl_ctrl_c_handler(int sig)
{
	if (sig == SIGINT) {
		killed = true;
	}
}

static int bl_setup_signal_handler(void)
{
	struct sigaction act = {
		.sa_handler = bl_ctrl_c_handler,
	};
	int ret;

	ret = sigemptyset(&act.sa_mask);
	if (ret == -1) {
		fprintf(stderr, "sigemptyset call failed: %s\n",
				strerror(errno));
		return EXIT_FAILURE;
	}

	ret = sigaddset(&act.sa_mask, SIGINT);
	if (ret == -1) {
		fprintf(stderr, "sigaddset call failed: %s\n",
				strerror(errno));
		return EXIT_FAILURE;
	}

	ret = sigaction(SIGINT, &act, NULL);
	if (ret == -1) {
		fprintf(stderr, "Failed to set SIGINT handler: %s\n",
				strerror(errno));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
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

	if (bl_setup_signal_handler() != EXIT_SUCCESS) {
		return EXIT_FAILURE;
	}

	return cmd_fn(argc, argv);
}
