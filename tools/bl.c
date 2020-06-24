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
#include "common.h"
#include "conversion.h"

/* Whether we've had a `ctrl-c`. */
volatile bool killed;

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
	bl_msg_print(msg, stdout);

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

	if (check_fit(led_mask, sizeof(msg.led.led_mask)) == false) {
		fprintf(stderr, "Value too large for message.\n");
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

static int bl_cmd_gains(int argc, char *argv[])
{
	union bl_msg_data msg = {
		.gain = {
			.type = BL_MSG_SET_GAINS,
		}
	};
	unsigned arg_gain_count;
	int ret;
	int dev_fd;
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
				"  \t[GAIN]\n",
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

		msg.gain.gain[i] = gain;
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


static int bl_cmd_oversample(int argc, char *argv[])
{
	union bl_msg_data msg = {
		.oversample = {
			.type = BL_MSG_SET_OVERSAMPLE,
		}
	};
	uint32_t oversample;
	int ret;
	int dev_fd;
	enum {
		ARG_PROG,
		ARG_CMD,
		ARG_DEV_PATH,
		ARG_OVERSAMPLE,
		ARG__COUNT,
	};

	
	if (argc != ARG__COUNT) {
		fprintf(stderr, "Usage:\n");
		fprintf(stderr, "  %s %s \\\n"
				"  \t<DEVICE_PATH> \\\n"
				"  \t<OVERSAMPLE>\n",
				argv[ARG_PROG],
				argv[ARG_CMD]);
		fprintf(stderr, "\n");
		fprintf(stderr, "Provide the numer of samples to read per reported sample\n");
		return EXIT_FAILURE;
	}

	if (read_uint32_t(argv[ARG_OVERSAMPLE], &oversample) == false){
		fprintf(stderr, "Failed to parse value.\n");
		return EXIT_FAILURE;
	}
	msg.oversample.oversample = oversample;

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

static int bl_cmd_offset(int argc, char *argv[])
{
	union bl_msg_data msg = {
		.offset = {
			.type = BL_MSG_SET_FIXEDOFFSET,
		}
	};
	uint32_t offset;
	int ret;
	int dev_fd;
	enum {
		ARG_PROG,
		ARG_CMD,
		ARG_DEV_PATH,
		ARG_OFFSET,
		ARG__COUNT,
	};

	
	if (argc != ARG__COUNT) {
		fprintf(stderr, "Usage:\n");
		fprintf(stderr, "  %s %s \\\n"
				"  \t<DEVICE_PATH> \\\n"
				"  \t<OFFSET>\n",
				argv[ARG_PROG],
				argv[ARG_CMD]);
		fprintf(stderr, "\n");
		fprintf(stderr, "Provide the amount to subtract from accumulated values in-chip\n");
		return EXIT_FAILURE;
	}

	
	if (read_uint32_t(argv[ARG_OFFSET], &offset) == false){
		fprintf(stderr, "Failed to parse value.\n");
		return EXIT_FAILURE;
	}
	msg.offset.offset = offset;

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
		fprintf(stderr, "  %s %s <DEVICE_PATH>\n",
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

int bl_cmd_receive_and_print_xy_loop(int dev_fd, int frequency)
{
	int ret = EXIT_SUCCESS;
	long current_x[MAX_CHANNELS] = {0};
	do {
		union bl_msg_data * msg = NULL;
		ret = bl_cmd_read_message(dev_fd, 10000, &msg);
		if (ret == EINTR) {
			killed = true;
		} else if (ret != 0) {
			ret = EXIT_FAILURE;
		}
		bl_live_samples_to_xy(msg, stdout, current_x, frequency);
	} while (!killed);
	return ret;
}


typedef enum bl_data_presentation
{
	HUMAN_READABLE,
	X_Y,
} bl_data_presentation;

static int bl_cmd_start_stream(
		int argc,
		char *argv[],
		bl_data_presentation present)
{
	union bl_msg_data msg = {
		.start = {
			.type = BL_MSG_START,
		}
	};
	uint32_t src_mask;
	uint32_t frequency;
	int ret;
	int dev_fd;
	enum {
		ARG_PROG,
		ARG_CMD,
		ARG_DEV_PATH,
		ARG_FREQUENCY,
		ARG_SRC_MASK,
		ARG__COUNT,
	};

	if (argc != ARG__COUNT) {
		fprintf(stderr, "Usage:\n");
		fprintf(stderr, "  %s %s \\\n"
				"  \t<DEVICE_PATH> \\\n"
				"  \t<FREQUENCY> \\\n"
				"  \t<SRC_MASK>\n",
				argv[ARG_PROG],
				argv[ARG_CMD]);
		fprintf(stderr, "\n");
		fprintf(stderr, "FREQUENCY is the sampling rate in Hz.\n");
		return EXIT_FAILURE;
	}

	if (read_uint32_t(argv[ARG_SRC_MASK],   &src_mask)   == false ||
	    read_uint32_t(argv[ARG_FREQUENCY],  &frequency)  == false) {
		fprintf(stderr, "Failed to parse value.\n");
		return EXIT_FAILURE;
	}

	if (check_fit(src_mask,   sizeof(msg.start.src_mask))   == false) {
		fprintf(stderr, "Value too large for message.\n");
		return EXIT_FAILURE;
	}

	msg.start.frequency = frequency;
	msg.start.src_mask = src_mask;

	dev_fd = bl_open_device(argv[ARG_DEV_PATH]);
	if (dev_fd == -1) {
		return EXIT_FAILURE;
	}
	if (present == HUMAN_READABLE) {
		bl_msg_print(&msg, stdout);
	}
	
	ret = bl_cmd_send(&msg, argv[ARG_DEV_PATH], dev_fd);

	if (ret != BL_ERROR_NONE) {
		bl_close_device(dev_fd);
		return EXIT_FAILURE;
	}

	if (present == HUMAN_READABLE) {
		ret = bl_cmd_receive_and_print_loop(dev_fd);
	}else if (present == X_Y) {
		ret = bl_cmd_receive_and_print_xy_loop(dev_fd, frequency);
	}

	/* Send abort after ctrl+c */
	if (killed) {
		union bl_msg_data abort_msg = {
			.type = BL_MSG_ABORT,
		};
		bl_cmd_send(&abort_msg, argv[ARG_DEV_PATH],dev_fd);
		if (present == HUMAN_READABLE) {
			bl_cmd_read_and_print_message(dev_fd, 10000);
		} 
	}

	bl_close_device(dev_fd);
	return ret;

}

int bl_cmd_start(int argc, char *argv[])
{
	return bl_cmd_start_stream(argc, argv, HUMAN_READABLE);
}

int bl_cmd_start_xy(int argc, char *argv[])
{
	return bl_cmd_start_stream(argc, argv, X_Y);
}

static int bl_cmd_continue(int argc, char *argv[])
{
	int dev_fd;
	int ret;
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

	dev_fd = bl_open_device(argv[ARG_DEV_PATH]);
	if (dev_fd == -1) {
		return EXIT_FAILURE;
	}
	ret = bl_cmd_receive_and_print_loop(dev_fd);

	bl_close_device(dev_fd);
	return ret;
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
		.name = "start",
		.help = "Start an acquisition",
		.fn = bl_cmd_start,
	},
	{
		.name = "startxy",
		.help = "Start an acquisition and print data out in the format [source channel],[time ms],[value]",
		.fn = bl_cmd_start_xy,
	},
	{
		.name = "continue",
		.help = "Continue an acquisition",
		.fn = bl_cmd_continue,
	},
	{
		.name = "abort",
		.help = "Abort an acquisition",
		.fn = bl_cmd_abort,
	},
	{
		.name = "gains",
		.help = "set gains",
		.fn = bl_cmd_gains,
	},
	{
		.name = "oversample",
		.help = "set oversamples",
		.fn = bl_cmd_oversample,
	},
	{
		.name = "offset",
		.help = "set offset",
		.fn = bl_cmd_offset,
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

	if (bl_setup_signal_handler() != EXIT_SUCCESS) {
		return EXIT_FAILURE;
	}

	return cmd_fn(argc, argv);
}
