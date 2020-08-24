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

#ifndef BV_UTIL_H
#define BV_UTIL_H

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdbool.h>

/**
 * Helper to squash warnings about unused variables.
 *
 * \param[in]  _u  The variable that is unused.
 */
#define BV_UNUSED(_u) \
	((void)(_u))

/**
 * Helper to get the number of entires in an array.
 *
 * \param[in]  _a  Array to get the entry count for.
 * \return entry count of array.
 */
#define BV_ARRAY_LEN(_a) \
	((sizeof(_a)) / (sizeof(*_a)))

/**
 * Parse an unsigned value from a string.
 *
 * \param[in]  string  String to be parsed.
 * \param[in]  out     Returns the value parsed out of string on success.
 * \return true on success, false on error.
 */
static inline bool util_read_unsigned(
		const char *string,
		unsigned *out)
{
	unsigned long long temp;
	char *end = NULL;

	errno = 0;
	temp = strtoull(string, &end, 0);

	if (end == string || errno == ERANGE) {
		return false;
	}

	if (temp > (unsigned long long)UINT_MAX) {
		return false;
	}

	*out = (unsigned)temp;
	return true;
}

/**
 * Parse an double value from a string.
 *
 * \param[in]  string  String to be parsed.
 * \param[in]  out     Returns the value parsed out of string on success.
 * \return true on success, false on error.
 */
static inline bool util_read_double(
		const char *string,
		double *out)
{
	double temp;
	char *end = NULL;

	errno = 0;
	temp = strtod(string, &end);

	if (end == string || errno == ERANGE) {
		return false;
	}

	*out = temp;
	return true;
}

#endif
