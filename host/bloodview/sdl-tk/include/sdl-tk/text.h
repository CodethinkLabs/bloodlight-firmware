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
 * \brief Header for the Text module.
 *
 * This provides a simple interface for creating text textures.
 */

#ifndef SDL_TK_TEXT_H
#define SDL_TK_TEXT_H

#include <stdbool.h>

#include <SDL2/SDL.h>
#include <SDL_ttf.h>

#include "colour.h"

/**
 * An sdl-tk text object.
 */
struct sdl_tk_text {
	unsigned     w; /**< Texture width in pixels. */
	unsigned     h; /**< Texture height in pixels. */
	SDL_Texture *t; /**< An SDL texture containing text. */
};

/**
 * List of sdl-tk text sizes.
 */
typedef enum sdl_tk_text_size {
	SDL_TK_TEXT_SIZE_NORMAL,
	SDL_TK_TEXT_SIZE_LARGE,
	SDL_TK_TEXT_SIZE__COUNT,
} sdl_tk_text_size_t;

/**
 * Finalise the sdl-tk text module.
 */
void sdl_tk_text_fini(
		void);

/**
 * Initialise the sdl-tk text module.
 *
 * \param[in]  ren        SDL renderer object.
 * \param[in]  font_path  Path to font to use, or NULL.
 * \return true on success, or false on failure.
 */
bool sdl_tk_text_init(
		SDL_Renderer *ren,
		const char   *font_path);

/**
 * Get an sdl-tk text object from a string.
 *
 * \param[in]  string  String to create text object for.
 * \param[in]  colour  Colour to render the string.
 * \param[in]  size    Size to render the string.
 * \return an sdl-tk text object, or NULL on failure.
 */
struct sdl_tk_text *sdl_tk_text_create(
		const char         *string,
		SDL_Color           colour,
		sdl_tk_text_size_t  size);

/**
 * Find the length of a string in pixels.
 *
 * \param[in]  string  String to measure.
 * \param[in]  size    Size to render the string.
 * \return Length of string in pixels.
 */
unsigned sdl_tk_text_get_size(
		const char         *string,
		sdl_tk_text_size_t  size);

/**
 * Destroy an sdl-tk text object.
 *
 * \param[in]  text  The text object to destroy.
 */
void sdl_tk_text_destroy(
		struct sdl_tk_text *text);

#endif /* SDL_TK_TEXT_H */
