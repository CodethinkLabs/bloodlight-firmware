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
#include <errno.h>
#include <assert.h>

#include "util.h"

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

bool read_sized_uint(const char *value, uint32_t *out, size_t target_size)
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
