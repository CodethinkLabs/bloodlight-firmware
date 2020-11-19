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

#ifndef BL_COMMON_UTIL_H
#define BL_COMMON_UTIL_H

/**
 * Helper to squash warnings about unused variables.
 *
 * \param[in]  _u  The variable that is unused.
 */
#define BL_UNUSED(_u) \
	((void)(_u))

/**
 * Helper to get the number of entires in an array.
 *
 * \param[in]  _a  Array to get the entry count for.
 * \return entry count of array.
 */
#define BL_ARRAY_LEN(_a) \
	((sizeof(_a)) / (sizeof(*_a)))

#define BL_STATIC_ASSERT(e) \
{ \
	enum { \
		bl_static_assert_check = 1 / (!!(e)) \
	}; \
}

#endif

