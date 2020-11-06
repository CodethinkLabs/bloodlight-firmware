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
 * \brief Header for the Colour module.
 *
 * This provides easy access to the toolkit colour scheme.
 */

#ifndef SDL_TK_COLOUR_H
#define SDL_TK_COLOUR_H

#include <stdbool.h>

#include <SDL2/SDL.h>

/**
 * Colour palette.
 */
enum sdl_tk_colour {
	SDL_TK_COLOUR_BACKGROUND,
	SDL_TK_COLOUR_INTERFACE,
	SDL_TK_COLOUR_SELECTION,
	SDL_TK_COLOUR_DISABLED,
	SDL_TK_COLOUR_SEL_DIS,
	SDL_TK_COLOUR__COUNT,
};

/**
 * Initialise the colour module.
 *
 * \return true on success or false on error.
 */
bool sdl_tk_colour_init(void);

/**
 * Finalise the colour module.
 */
void sdl_tk_colour_fini(void);

/**
 * Get an interface palette colour.
 *
 * \param[in]  col  The interface colour to get.
 * \return the interface colour.
 */
SDL_Color sdl_tk_colour_get(enum sdl_tk_colour col);

/**
 * Convert HSV values to an SDL_Colour.
 *
 * \param[in]  h  Hue: Range 0-360
 * \param[in]  s  Saturation: Range 0-100
 * \param[in]  v  Value: Range 0-100
 * \return An SDL_Color object.
 */
SDL_Color sdl_tk_colour_get_hsv(uint32_t h, uint32_t s, uint32_t v);

#endif /* SDL_TK_COLOUR_H */
