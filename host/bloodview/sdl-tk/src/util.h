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

/**
 * Shift a rectangle so it's contained in a bounding rectangle.
 *
 * If the bounding rectangle is too small, the top left of the inner
 * rectangle is kept inside the bounding rect.
 *
 * \param[in]      bound  Bounding rectangle.
 * \param[in,out]  rect   Rectangle to update position of.
 */
static inline void sdl_tl__shift_rect(
		const SDL_Rect *bound,
		SDL_Rect *rect)
{
	if (rect->x + rect->w > bound->x + bound->w) {
		rect->x -= (rect->x + rect->w) - (bound->x + bound->w);
	}
	if (rect->y + rect->h > bound->y + bound->h) {
		rect->y -= (rect->y + rect->h) - (bound->y + bound->h);
	}
	if (rect->x < bound->x) {
		rect->x += bound->x - rect->x;
	}
	if (rect->y < bound->y) {
		rect->y += bound->y - rect->y;
	}
}

#endif
