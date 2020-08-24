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

#include <stdlib.h>

#include "util.h"

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

	ret = EXIT_SUCCESS;

	return ret;
}
