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

/**
 * \file
 * \brief Bloodview application implementation.
 *
 * This provides the main entry point from the OS, and high level functionality.
 */

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include <getopt.h>

#include "dpp/dpp.h"

#include "sdl.h"
#include "util.h"
#include "device.h"
#include "main-menu.h"

/** Bloodview global context data. */
static struct {
	volatile bool quit;
	volatile bool started;
	volatile device_state_t device_state;
} bloodview_g;

/* Exported interface, documented in bloodview.h */
void bloodview_start_cal_cb(void *pw)
{
	BV_UNUSED(pw);

	if (device_calibrate_start()) {
		sdl_main_menu_close();
	}
}

/* Exported interface, documented in bloodview.h */
void bloodview_start_acq_cb(void *pw)
{
	BV_UNUSED(pw);

	if (device_acquisition_start()) {
		sdl_main_menu_close();
	}
}

/* Exported interface, documented in bloodview.h */
void bloodview_stop_cb(void *pw)
{
	BV_UNUSED(pw);

	if (device_stop()) {
		/* Don't close menu here, because it's probably still wanted. */
	}
}

/* Exported interface, documented in bloodview.h */
void bloodview_quit_cb(void *pw)
{
	BV_UNUSED(pw);

	bloodview_g.quit = true;
}

/**
 * Device state change callback.
 *
 * This is called when the device modult reports a state change,
 *
 * \param[in]  pw     Unused private word.
 * \param[in]  state  The new device state.
 */
static void bloodview_device_state_change_cb(
		void *pw, device_state_t state)
{
	BV_UNUSED(pw);

	bloodview_g.device_state = state;

	if (bloodview_g.started == true) {
		main_menu_set_acq_available(state != DEVICE_STATE_ACTIVE);
	}
}

/** Bloodview commandline options. */
struct bv_options {
	const char *path_resources; /**< Directory to load resources from. */
	const char *path_config;    /**< Directory where configs are stored. */
	const char *file_config;    /**< Config filename to load on startup. */
	const char *path_font;      /**< Path to font file to use. */
};

/**
 * Parse the command line arguments.
 *
 * \param[in]  argc         Number of command line arguments.
 * \param[in]  argv         String vector of command line arguments.
 * \param[out] options_out  Returns parsed command line arguments.
 * \return true on success, false otherwise.
 */
static bool bloodview__parse_cli(
		int argc,
		char **argv,
		struct bv_options *options_out)
{
	struct bv_options opt = {
		.path_resources = "resources",
		.path_config    = "config",
	};
	enum options {
		BV_OPTTION_PATH_RESOURCES_DIR = 'R',
		BV_OPTTION_PATH_CONFIG_DIR    = 'C',
		BV_OPTTION_FILE_CONFIG        = 'c',
		BV_OPTTION_PATH_FONT          = 'f',
	};
	static const char optstr[] = "R:C:c:f:";
	static struct option options[] = {
		{
			.val = BV_OPTTION_PATH_RESOURCES_DIR,
			.name = "resources-dir",
			.has_arg = required_argument,
		},
		{
			.val = BV_OPTTION_PATH_CONFIG_DIR,
			.name = "config-dir",
			.has_arg = required_argument,
		},
		{
			.val = BV_OPTTION_FILE_CONFIG,
			.name = "config",
			.has_arg = required_argument,
		},
		{
			.val = BV_OPTTION_PATH_FONT,
			.name = "font",
			.has_arg = required_argument,
		},
		{
			.name = NULL,
		},
	};

	assert(options != NULL);

	opterr = 1; /* Let getopt print invalid arg messages */
	while (true) {
		int longindex = -1;
		int c = getopt_long(argc, argv, optstr, options, &longindex);
		if (c == -1)
			break;
		if (c == '?' || c == ':') {
			/* Invalid option or missing argument. */
			return false;
		}
		enum options option = c;
		switch (option) {
		case BV_OPTTION_PATH_RESOURCES_DIR:
			opt.path_resources = optarg;
			break;

		case BV_OPTTION_PATH_CONFIG_DIR:
			opt.path_config = optarg;
			break;

		case BV_OPTTION_FILE_CONFIG:
			opt.file_config = optarg;
			break;

		case BV_OPTTION_PATH_FONT:
			opt.path_font = optarg;
			break;
		}
	}
	if (optind != argc) {
		fprintf(stderr, "%s: Unexpected arguments\n", argv[0]);
		return false;
	}

	*options_out = opt;

	return true;
}

/**
 * Main entry point from OS.
 *
 * \param[in]  argc  Number of command line parameters.
 * \param[in]  argv  String vector of command line arguments.
 * \return Exit code.
 */
int main(int argc, char *argv[])
{
	struct bv_options options;
	int ret = EXIT_FAILURE;

	if (bloodview__parse_cli(argc, argv, &options) == false) {
		return EXIT_FAILURE;
	}

	if (!dpp_init(options.path_resources)) {
		return EXIT_FAILURE;
	}

	if (!device_init(NULL, bloodview_device_state_change_cb, NULL)) {
		dpp_fini();
		return EXIT_FAILURE;
	}

	if (!sdl_init(options.path_resources,
			options.path_config,
			options.file_config,
			options.path_font)) {
		device_fini();
		dpp_fini();
		return EXIT_FAILURE;
	}

	bloodview_g.started = true;
	main_menu_set_acq_available(
			bloodview_g.device_state != DEVICE_STATE_ACTIVE);

	while (!bloodview_g.quit && sdl_handle_input()) {
		sdl_present();
	}

	ret = EXIT_SUCCESS;

	device_fini();
	sdl_fini();
	dpp_fini();

	return ret;
}
