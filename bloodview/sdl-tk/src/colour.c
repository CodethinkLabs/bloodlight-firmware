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
 * \brief Implementation of the Colour module.
 *
 * Generates the sdl-tk colour scheme.
 */

#include "sdl-tk/colour.h"

/**
 * Interface palette.
 */
static SDL_Color sdl_tk_colours[SDL_TK_COLOUR__COUNT];

/**
 * Force a value into the range 0-255.
 *
 * \param[in]  val  Value to rescale.
 * \param[in]  max  Maximum value for input value.
 * \return rescaled value.
 */
static inline uint32_t rescale_255(uint32_t val, uint32_t max)
{
	if (val > max) {
		val = max;
	}

	return (val * 255 + max / 2) / max;
}

/**
 * Convert HSV values to an SDL_Colour.
 *
 * \param[in]  h  Hue: Range 0-360
 * \param[in]  s  Saturation: Range 0-100
 * \param[in]  v  Value: Range 0-100
 * \return An SDL_Color object.
 */
static SDL_Color sdl_tk_colour__get_hsv(uint32_t h, uint32_t s, uint32_t v)
{
	uint8_t sector, remainder, p, q, t;

	h = rescale_255(h, 360);
	s = rescale_255(s, 100);
	v = rescale_255(v, 100);

	if (s == 0) {
		return (SDL_Color) { .r = v, .g = v, .b = v };
	}

	sector = h / 43;
	remainder = (h - sector * 43) * 6;

	p = (v * (255 - ( s                          ))) >> 8;
	q = (v * (255 - ((s * (      remainder)) >> 8))) >> 8;
	t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

	switch (sector) {
	case 0:  return (SDL_Color) { .r = v, .g = t, .b = p };
	case 1:  return (SDL_Color) { .r = q, .g = v, .b = p };
	case 2:  return (SDL_Color) { .r = p, .g = v, .b = t };
	case 3:  return (SDL_Color) { .r = p, .g = q, .b = v };
	case 4:  return (SDL_Color) { .r = t, .g = p, .b = v };
	default: return (SDL_Color) { .r = v, .g = p, .b = q };
	}
}

/* Exported function, documented in colour.h */
bool sdl_tk_colour_init(void)
{
	sdl_tk_colours[SDL_TK_COLOUR_BACKGROUND] = sdl_tk_colour__get_hsv(  0,   0,   0);
	sdl_tk_colours[SDL_TK_COLOUR_INTERFACE]  = sdl_tk_colour__get_hsv(225,  70, 100);
	sdl_tk_colours[SDL_TK_COLOUR_SELECTION]  = sdl_tk_colour__get_hsv( 30,  65, 100);
	sdl_tk_colours[SDL_TK_COLOUR_DISABLED]   = sdl_tk_colour__get_hsv(225,  70,  50);
	sdl_tk_colours[SDL_TK_COLOUR_SEL_DIS]    = sdl_tk_colour__get_hsv( 30,  65,  50);

	return true;
}

/* Exported function, documented in colour.h */
void sdl_tk_colour_fini(void)
{
	/* No-op for now. */
	return;
}

/* Exported function, documented in colour.h */
SDL_Color sdl_tk_colour_get(enum sdl_tk_colour col)
{
	return sdl_tk_colours[col];
}
