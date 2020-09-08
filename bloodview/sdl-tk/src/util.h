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
 * \brief Header for general utility functionality.
 *
 * Simple functionality that doesn't deserve its own module.
 */

#ifndef UTIL_H
#define UTIL_H

/**
 * Helper to squash warnings about unused variables.
 *
 * \param[in]  _u  The variable that is unused.
 */
#define SDL_TK_UNUSED(_u) \
	((void)(_u))

/**
 * Helper to get the number of entires in an array.
 *
 * \param[in]  _a  Array to get the entry count for.
 * \return entry count of array.
 */
#define SDL_TK_ARRAY_LEN(_a) \
	((sizeof(_a)) / (sizeof(*_a)))

#endif
