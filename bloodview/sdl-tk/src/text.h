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

#ifndef TEXT_H
#define TEXT_H

#include "sdl-tk/text.h"

/** List of common text entries. */
typedef enum sdl_tk_text_common {
	SDL_TK_TEXT_ARROW_RIGHT,
	SDL_TK_TEXT_OFF,
	SDL_TK_TEXT_ON,
	SDL_TK_TEXT__COUNT
} sdl_tk_text_common_t;

/**
 * Get the sdl-tk text object for an common text entry.
 *
 * The returned sdl-tk text object remains valid until the sdl-tk text module
 * is finalised.
 *
 * \param[in]  col     Colour to get.
 * \param[in]  size    Size to get.
 * \param[in]  common  Common text entry to get.
 * \return an sdl-tk text object borrow on success, or NULL on failure.
 */
const struct sdl_tk_text *sdl_tk_text_get_common(
		enum sdl_tk_colour   col,
		sdl_tk_text_size_t   size,
		sdl_tk_text_common_t common);

#endif /* TEXT_H */
