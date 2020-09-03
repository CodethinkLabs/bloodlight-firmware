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
 * \brief Implementation of the text module.
 *
 * This provides a simple interface for creating text textures.
 */

#include <stdio.h>

#include "text.h"
#include "util.h"

/** An sdk-tk text font. */
struct sdl_font {
	TTF_Font *font; /**< A font handle. */
	unsigned  size; /**< Pt size of the font. */
};

/** Array of common text entries at various sizes and in various colours. */
static struct sdl_tk_text *sdl_tk_text_common
		[SDL_TK_TEXT_SIZE__COUNT]
		[SDL_TK_COLOUR__COUNT]
		[SDL_TK_TEXT__COUNT];

/** Array of sdl-tk text fonts at multiple sizes. */
static struct sdl_font sdl_font[] = {
	[SDL_TK_TEXT_SIZE_NORMAL] = { .font = NULL, .size = 18 },
	[SDL_TK_TEXT_SIZE_LARGE]  = { .font = NULL, .size = 48 },
};

/** Text module global reference to the SDL renderer. */
static SDL_Renderer *sdl_ren_g;

/**
 * Free the common text objects.
 */
static void sdl_tk_text__common_fini(void)
{
	for (unsigned i = 0; i < SDL_TK_TEXT_SIZE__COUNT; i++) {
		for (unsigned j = 0; j < SDL_TK_COLOUR__COUNT; j++) {
			for (unsigned k = 0; k < SDL_TK_TEXT__COUNT; k++) {
				sdl_tk_text_destroy(sdl_tk_text_common[i][j][k]);
				sdl_tk_text_common[i][j][k] = NULL;
			}
		}
	}
}

/**
 * Initalise the array of common text objects.
 */
static bool sdl_tk_text__common_init(void)
{
	static const char *str[] = {
		[SDL_TK_TEXT_ARROW_RIGHT] = ">",
		[SDL_TK_TEXT_OFF]         = NULL,
		[SDL_TK_TEXT_ON]          = "On",
	};

	for (unsigned i = 0; i < SDL_TK_TEXT_SIZE__COUNT; i++) {
		for (unsigned j = 0; j < SDL_TK_COLOUR__COUNT; j++) {
			for (unsigned k = 0; k < SDL_TK_TEXT__COUNT; k++) {
				if (str[k] == NULL) {
					continue;
				}
				sdl_tk_text_common[i][j][k] = sdl_tk_text_create(
						str[k],
						sdl_tk_colour_get(j),
						i);
				if (sdl_tk_text_common[i][j][k] == NULL) {
					goto error;
				}
			}
		}
	}

	return true;

error:
	sdl_tk_text__common_fini();
	return false;
}

/* Exported function, documented in include/sdl-tk/text.h. */
void sdl_tk_text_fini(void)
{
	for (unsigned i = 0; i < SDL_TK_ARRAY_LEN(sdl_font); i++) {
		if (sdl_font[i].font != NULL) {
			TTF_CloseFont(sdl_font[i].font);
			sdl_font[i].font = NULL;
		}
	}

	if (TTF_WasInit()) {
		TTF_Quit();
	}

	sdl_tk_text__common_fini();

	sdl_ren_g = NULL;
}

/* Exported function, documented in include/sdl-tk/text.h. */
bool sdl_tk_text_init(
		SDL_Renderer *ren,
		const char *font_path)
{
	if (font_path == NULL) {
		font_path = "/usr/share/fonts/truetype/freefont/FreeSans.ttf";
	}

	if (TTF_Init() != 0) {
		printf("TTF_Init Error: %s\n",
				TTF_GetError());
		goto error;
	}

	for (unsigned i = 0; i < SDL_TK_ARRAY_LEN(sdl_font); i++) {
		sdl_font[i].font = TTF_OpenFont(font_path, sdl_font[i].size);
		if (sdl_font[i].font == NULL) {
			printf("TTF_OpenFont Error: %s\n",
					TTF_GetError());
			goto error;
		}
	}

	sdl_ren_g = ren;

	if (!sdl_tk_text__common_init()) {
		goto error;
	}

	return true;

error:
	sdl_tk_text_fini();
	return false;
}

/* Exported function, documented in include/sdl-tk/text.h. */
void sdl_tk_text_destroy(
		struct sdl_tk_text *text)
{
	if (text != NULL) {
		if (text->t != NULL) {
			SDL_DestroyTexture(text->t);
		}
		free(text);
	}
}

/* Exported function, documented in include/sdl-tk/text.h. */
struct sdl_tk_text *sdl_tk_text_create(
		const char      *string,
		SDL_Color        colour,
		sdl_tk_text_size_t  size)
{
	struct sdl_tk_text *text;
	SDL_Surface *surface;
	int w, h;

	text = calloc(1, sizeof(*text));
	if (text == NULL) {
		return NULL;
	}

	surface = TTF_RenderText_Blended(sdl_font[size].font, string, colour);
	if (surface == NULL) {
		fprintf(stderr, "TTF_RenderText_Blended Error: %s\n",
				TTF_GetError());
		goto error;
	}

	text->t = SDL_CreateTextureFromSurface(sdl_ren_g, surface);
	SDL_FreeSurface(surface);
	if (text->t == NULL) {
		fprintf(stderr, "SDL_CreateTextureFromSurface Error: %s\n",
				SDL_GetError());
		goto error;
	}

	if (SDL_QueryTexture(text->t, NULL, NULL, &w, &h) != 0) {
		fprintf(stderr, "SDL_QueryTexture Error: %s\n",
				SDL_GetError());
		goto error;
	}

	text->w = w;
	text->h = h;

	return text;

error:
	sdl_tk_text_destroy(text);
	return NULL;
}

/* Exported function, documented in src/text.h. */
const struct sdl_tk_text *sdl_tk_text_get_common(
		enum sdl_tk_colour   col,
		sdl_tk_text_size_t   size,
		sdl_tk_text_common_t common)
{
	return sdl_tk_text_common[size][col][common];
}
