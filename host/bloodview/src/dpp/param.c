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
 * \brief Implementation of the data processing pipeline filter handling.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "param.h"

/** Exported function, documented in param.h */
const struct bv_param *param_lookup(
		const struct bv_param *param,
		unsigned param_count,
		const char *name,
		enum bv_value_e kind)
{
	for (unsigned i = 0; i < param_count; i++) {
		if (strcmp(param[i].name, name) == 0) {
			if (param[i].value.type != kind) {
				fprintf(stderr, "Error: Bad type for %s.\n",
						name);
				return NULL;
			}
			return &param[i];
		}
	}

	fprintf(stderr, "Error: Parameter '%s' not provided.\n", name);

	return NULL;
}

/** Exported function, documented in param.h */
const struct bv_param_spec *param_spec_lookup(
		const struct bv_param_spec *param,
		unsigned param_count,
		const char *name)
{
	for (unsigned i = 0; i < param_count; i++) {
		if (strcmp(param[i].name, name) == 0) {
			return &param[i];
		}
	}

	fprintf(stderr, "Error: Parameter spec for '%s' not provided.\n", name);

	return NULL;
}
