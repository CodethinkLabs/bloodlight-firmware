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

#ifndef RENDER_H
#define RENDER_H

/**
 * Render a rectangle, optionally setting the colour to use.
 *
 * \param[in]  ren  SDL renderer to use.
 * \param[in]  col  Colour to set, or NULL.
 * \param[in]  r    Rectangle to render.
 */
static inline void sdl_tk_render_rect(
		SDL_Renderer      *ren,
		SDL_Color         *col,
		SDL_Rect          *r)
{
	if (col != NULL) {
		SDL_SetRenderDrawColor(ren, col->r, col->g, col->b, 255);
	}
	SDL_RenderFillRect(ren, r);
}

#endif /* RENDER_H */
