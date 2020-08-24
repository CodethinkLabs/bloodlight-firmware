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

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include "sdl.h"
#include "util.h"
#include "main-menu.h"

/** Bloodview global context data. */
static struct {
	bool quit;
} bloodview_g;

/* Exported interface, documented in bloodview.h */
void bloodview_start_cal_cb(void *pw)
{
	BV_UNUSED(pw);
}

/* Exported interface, documented in bloodview.h */
void bloodview_start_acq_cb(void *pw)
{
	BV_UNUSED(pw);
}

/* Exported interface, documented in bloodview.h */
void bloodview_stop_cb(void *pw)
{
	BV_UNUSED(pw);
}

/* Exported interface, documented in bloodview.h */
void bloodview_quit_cb(void *pw)
{
	BV_UNUSED(pw);

	bloodview_g.quit = true;
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
	int ret = EXIT_FAILURE;

	BV_UNUSED(argc);
	BV_UNUSED(argv);

	if (!sdl_init(NULL)) {
		return EXIT_FAILURE;
	}

	while (!bloodview_g.quit && sdl_handle_input()) {
		sdl_present();
	}

	ret = EXIT_SUCCESS;

	sdl_fini();

	return ret;
}