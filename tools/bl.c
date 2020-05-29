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

#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <poll.h>

#include "../src/error.h"
#include "../src/util.h"
#include "../src/msg.h"

typedef int (* bl_cmd_fn)(int argc, char *argv[]);

static inline bool read_uint32_t(
		const char *value,
		uint32_t *out)
{
	unsigned long long temp;
	char *end = NULL;

	errno = 0;
	temp = strtoull(value, &end, 0);

	if (end == value || errno == ERANGE || temp > UINT32_MAX) {
		return false;
	}

	*out = (uint32_t)temp;
	return true;
}

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

static inline const char *bl_msg_type_to_str(enum bl_msg_type type)
{
	static const char *types[] = {
		[BL_MSG_RESPONSE]      = "Response",
		[BL_MSG_LED_TEST]      = "LED Test",
		[BL_MSG_ACQ_SETUP]     = "Acq Setup",
		[BL_MSG_ACQ_SET_GAINS] = "Acq Set Gains",
		[BL_MSG_ACQ_START]     = "Acq Start",
		[BL_MSG_ACQ_ABORT]     = "Acq Abort",
		[BL_MSG_RESET]         = "Reset",
		[BL_MSG_SAMPLE_DATA]   = "Sample Data",
	};

	if (type > BL_ARRAY_LEN(types)) {
		return NULL;
	}

	return types[type];
}

static void bl_msg_print(union bl_msg_data *msg)
{
	if (bl_msg_type_to_str(msg->type) == NULL) {
		fprintf(stderr, "- Type: Unknown (0x%x)\n",
				(unsigned) msg->type);
		return;
	}

	fprintf(stdout, "- Type: %s\n", bl_msg_type_to_str(msg->type));

	switch (msg->type) {
	case BL_MSG_RESPONSE:
		if (bl_msg_type_to_str(msg->response.response_to) == NULL) {
			fprintf(stderr, "- Response to: Unknown (0x%x)\n",
					(unsigned) msg->response.response_to);
		} else {
			fprintf(stdout, "  Response to: %s\n",
					bl_msg_type_to_str(
						msg->response.response_to));
		}
		fprintf(stdout, "  Error code: %u\n",
				(unsigned) msg->response.error_code);
		break;

	case BL_MSG_SAMPLE_DATA:
		fprintf(stdout, "  Count: %u\n",
				(unsigned) msg->sample_data.count);
		fprintf(stdout, "  Source mask: 0x%x\n",
				(unsigned) msg->sample_data.src_mask);
		fprintf(stdout, "  Data:\n");
		for (unsigned i = 0; i < msg->sample_data.count; i++) {
			fprintf(stdout, "  - %u\n",
				(unsigned) msg->sample_data.data[i]);
		}
		break;

	default:
		break;
	}
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

	while (total_read != size) {
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

static uint16_t bl_response_data[sizeof(union bl_msg_data) / 2 + 1 + 64];

static int bl_cmd_read_message(int dev_fd, int timeout_ms)
{
	union bl_msg_data * const msg = (void *) &bl_response_data[0];

	ssize_t expected_len;
	ssize_t read_len;

	expected_len = sizeof(msg->type);
	read_len = bl_read(dev_fd, &msg->type, expected_len, timeout_ms);
	if (read_len != expected_len) {
		fprintf(stderr, "Failed to read message type from device");
		if (read_len < 0)
			fprintf(stderr, ": %s", strerror(-read_len));
		fprintf(stderr, "\n");
		return EXIT_FAILURE;
	}

	expected_len = bl_msg_type_to_len(msg->type) - sizeof(msg->type);
	read_len = bl_read(dev_fd, &msg->type + sizeof(msg->type),
			expected_len, timeout_ms);
	if (read_len != expected_len) {
		fprintf(stderr, "Failed to read message body from device");
		if (read_len < 0)
			fprintf(stderr, ": %s", strerror(-read_len));
		fprintf(stderr, "\n");
		return EXIT_FAILURE;
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
			return EXIT_FAILURE;
		}
	}

	bl_msg_print(msg);

	return EXIT_SUCCESS;
}

static int bl_cmd_send(
		const union bl_msg_data *msg,
		const char *dev_path, bool multi)
{
	int dev_fd;
	ssize_t written;
	int ret = EXIT_SUCCESS;

	dev_fd = open(dev_path, O_RDWR);
	if (dev_fd == -1) {
		fprintf(stderr, "Failed to open '%s': %s\n",
				dev_path, strerror(errno));
		return EXIT_FAILURE;
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
		}
	} else {
		fprintf(stderr, "Failed get terminal attributes"
				" of '%s': %s\n", dev_path, strerror(errno));
	}

	if (msg != NULL)
	{
		written = write(dev_fd, msg, bl_msg_type_to_len(msg->type));
		if (written != bl_msg_type_to_len(msg->type)) {
			fprintf(stderr, "Failed write message to '%s'\n", dev_path);
			ret = EXIT_FAILURE;
			goto cleanup;
		}
	}

	do {
		if (bl_cmd_read_message(dev_fd, 1000) != EXIT_SUCCESS) {
			ret = EXIT_FAILURE;
			goto cleanup;
		}
	} while (multi);

cleanup:
	close(dev_fd);
	return ret;
}

static int bl_cmd_led_test(int argc, char *argv[])
{
	uint32_t led_mask;
	union bl_msg_data msg = {
		.led_test = {
			.type = BL_MSG_LED_TEST,
		}
	};
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
				"  \t<DEVICE_PATH> \\\n"
				"  \t<LED_MASK>\n",
				argv[ARG_PROG],
				argv[ARG_CMD]);
		return EXIT_FAILURE;
	}

	if (read_uint32_t(argv[ARG_LED_MASK], &led_mask) == false) {
		fprintf(stderr, "Failed to parse value.\n");
		return EXIT_FAILURE;
	}

	if (check_fit(led_mask, sizeof(msg.led_test.led_mask)) == false) {
		fprintf(stderr, "Value too large for message.\n");
		return EXIT_FAILURE;
	}

	msg.led_test.led_mask = led_mask;

	return bl_cmd_send(&msg, argv[ARG_DEV_PATH], false);
}

static int bl_cmd__no_params_helper(
		int argc,
		char *argv[],
		enum bl_msg_type type)
{
	union bl_msg_data msg = {
		.type = type,
	};
	enum {
		ARG_PROG,
		ARG_CMD,
		ARG_DEV_PATH,
		ARG__COUNT,
	};

	if (argc != ARG__COUNT) {
		fprintf(stderr, "Usage:\n");
		fprintf(stderr, "  %s %s <DEVICE_PATH>\n",
				argv[ARG_PROG], argv[ARG_CMD]);
		return EXIT_FAILURE;
	}

	return bl_cmd_send(&msg, argv[ARG_DEV_PATH],
		type == BL_MSG_ACQ_START);
}

static int bl_cmd_acq_setup(int argc, char *argv[])
{
	union bl_msg_data msg = {
		.acq_setup = {
			.type = BL_MSG_ACQ_SETUP,
		}
	};
	uint32_t src_mask;
	uint32_t samples;
	uint32_t rate;
	enum {
		ARG_PROG,
		ARG_CMD,
		ARG_DEV_PATH,
		ARG_RATE,
		ARG_SAMPLES,
		ARG_SRC_MASK,
		ARG__COUNT,
	};

	if (argc != ARG__COUNT) {
		fprintf(stderr, "Usage:\n");
		fprintf(stderr, "  %s %s \\\n"
				"  \t<DEVICE_PATH> \\\n"
				"  \t<RATE> \\\n"
				"  \t<SAMPLES> \\\n"
				"  \t<SRC_MASK>\n",
				argv[ARG_PROG],
				argv[ARG_CMD]);
		return EXIT_FAILURE;
	}

	if (read_uint32_t(argv[ARG_SRC_MASK], &src_mask) == false ||
	    read_uint32_t(argv[ARG_SAMPLES],  &samples)  == false ||
	    read_uint32_t(argv[ARG_RATE],     &rate)     == false) {
		fprintf(stderr, "Failed to parse value.\n");
		return EXIT_FAILURE;
	}

	if (check_fit(src_mask, sizeof(msg.acq_setup.src_mask)) == false ||
	    check_fit(samples,  sizeof(msg.acq_setup.samples))  == false ||
	    check_fit(rate,     sizeof(msg.acq_setup.rate))     == false) {
		fprintf(stderr, "Value too large for message.\n");
		return EXIT_FAILURE;
	}

	msg.acq_setup.rate = rate;
	msg.acq_setup.samples = samples;
	msg.acq_setup.src_mask = src_mask;

	return bl_cmd_send(&msg, argv[ARG_DEV_PATH], false);
}

static int bl_cmd_acq_set_gains(int argc, char *argv[])
{
	union bl_msg_data msg = {
		.acq_setup = {
			.type = BL_MSG_ACQ_SET_GAINS,
		}
	};
	unsigned arg_gain_count;
	enum {
		ARG_PROG,
		ARG_CMD,
		ARG_DEV_PATH,
		ARG__COUNT,
	};

	arg_gain_count = argc - ARG__COUNT;

	if (argc < ARG__COUNT || arg_gain_count > BL_ACQ_PD__COUNT) {
		fprintf(stderr, "Usage:\n");
		fprintf(stderr, "  %s %s \\\n"
				"  \t<DEVICE_PATH> \\\n"
				"  \t[GAIN]*\n",
				argv[ARG_PROG],
				argv[ARG_CMD]);
		fprintf(stderr, "\n");
		fprintf(stderr, "GAIN values which are not provided, "
				"will default to 1 (no amplification).\n");
		fprintf(stderr, "\n");
		fprintf(stderr, "A GAIN value must be a power of two"
				" up to 16.\n");
		fprintf(stderr, "\n");
		fprintf(stderr, "Up to %u GAIN values may be provided.\n",
				(unsigned) BL_ACQ_PD__COUNT);
		fprintf(stderr, "If a single GAIN value is given it is used "
				"for all photodiodes.\n");
		return EXIT_FAILURE;
	}

	for (unsigned i = 0; i < BL_ACQ_PD__COUNT; i++) {
		uint32_t gain;

		if (arg_gain_count == 0) {
			gain = 1;
		} else {
			unsigned arg = i % arg_gain_count;
			const char *arg_s = argv[ARG__COUNT + arg];

			if (!read_uint32_t(arg_s, &gain)) {
				fprintf(stderr, "Failed to parse value.\n");
				return EXIT_FAILURE;
			}

			if (!check_fit(gain, sizeof(gain))) {
				fprintf(stderr, "Value too large for message.\n");
				return EXIT_FAILURE;
			}
		}

		msg.acq_set_gains.gain[i] = gain;
	}

	return bl_cmd_send(&msg, argv[ARG_DEV_PATH], false);
}

static int bl_cmd_acq_start(int argc, char *argv[])
{
	return bl_cmd__no_params_helper(argc, argv, BL_MSG_ACQ_START);
}

static int bl_cmd_acq_continue(int argc, char *argv[])
{
	enum {
		ARG_PROG,
		ARG_CMD,
		ARG_DEV_PATH,
		ARG__COUNT,
	};

	if (argc != ARG__COUNT) {
		fprintf(stderr, "Usage:\n");
		fprintf(stderr, "  %s %s <DEVICE_PATH>\n",
				argv[ARG_PROG], argv[ARG_CMD]);
		return EXIT_FAILURE;
	}

	return bl_cmd_send(NULL, argv[ARG_DEV_PATH], true);
}

static int bl_cmd_acq_abort(int argc, char *argv[])
{
	return bl_cmd__no_params_helper(argc, argv, BL_MSG_ACQ_ABORT);
}

static int bl_cmd_reset(int argc, char *argv[])
{
	return bl_cmd__no_params_helper(argc, argv, BL_MSG_RESET);
}

static const struct bl_cmd {
	const char *name;
	const char *help;
	const bl_cmd_fn fn;
} cmds[] = {
	{
		.name = "led-test",
		.help = "Turn LEDs on/off",
		.fn = bl_cmd_led_test,
	},
	{
		.name = "acq-setup",
		.help = "Set up an acquisition",
		.fn = bl_cmd_acq_setup,
	},
	{
		.name = "acq-set-gains",
		.help = "Set up to 4 gains for given LED",
		.fn = bl_cmd_acq_set_gains,
	},
	{
		.name = "acq-start",
		.help = "Start an acquisition",
		.fn = bl_cmd_acq_start,
	},
	{
		.name = "acq-continue",
		.help = "Continue an acquisition",
		.fn = bl_cmd_acq_continue,
	},
	{
		.name = "acq-abort",
		.help = "Abort an acquisition",
		.fn = bl_cmd_acq_abort,
	},
	{
		.name = "reset",
		.help = "Reset the device",
		.fn = bl_cmd_reset,
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
