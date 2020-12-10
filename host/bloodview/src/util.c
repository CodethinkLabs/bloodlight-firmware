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
 * \brief Implementation of the util module.
 *
 * Simple functionality that doesn't deserve its own module.
 */

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

/* Exported interface, documented in util.c */
char *util_create_path(
		const char *dir_path,
		const char *filename)
{
	int n = 0;
	size_t size = 0;
	char *str = NULL;

	assert(filename != NULL);

	if (dir_path == NULL) {
		return strdup(filename);
	}

	/* Determine required size */
	n = snprintf(NULL, 0, "%s/%s", dir_path, filename);
	if (n < 0) {
		return NULL;
	}

	size = (size_t) n + 1;

	str = malloc(size);
	if (str == NULL) {
		return NULL;
	}

	n = snprintf(str, size, "%s/%s", dir_path, filename);
	if (n < 0) {
		free(str);
		return NULL;
	}

	return str;
}
