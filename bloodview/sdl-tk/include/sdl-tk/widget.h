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

#ifndef SDL_TK_WIDGET_H
#define SDL_TK_WIDGET_H

#include <stdbool.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "text.h"

struct sdl_tk_widget;

/**
 * Destroy an sdl-tk widget.
 *
 * \param[in]  widget  The widget to destroy.
 */
void sdl_tk_widget_destroy(
		struct sdl_tk_widget *widget);

/**
 * Render an sdl-tk widget.
 *
 * \param[in]  widget  The widget to render.
 * \param[in]  ren     SDL renderer to use.
 * \param[in]  x       X coordinate.
 * \param[in]  y       Y coordinate.
 */
void sdl_tk_widget_render(
		struct sdl_tk_widget *widget,
		SDL_Renderer         *ren,
		unsigned              x,
		unsigned              y);

/**
 * Fire an sdl-tk widget's action, if any.
 *
 * \param[in]  widget  The widget to activate.
 */
void sdl_tk_widget_action(
		struct sdl_tk_widget *widget);

/**
 * Layout an sdl-tk widget.
 *
 * \param[in]  widget  The widget to layout.
 */
void sdl_tk_widget_layout(
		struct sdl_tk_widget *widget);

/**
 * Fire an input event at an sdl-tk widget.
 *
 * \param[in]  widget  The widget to fire input at.
 * \param[in]  event   The input event to be handled.
 * \return true if the widget handled the input, false otherwise.
 */
bool sdl_tk_widget_input(
		struct sdl_tk_widget *widget,
		SDL_Event            *event);

/**
 * Set whether an sdl-tk widget has input focus.
 *
 * \param[in]  widget  The widget to set focus for.
 * \param[in]  focus   Whether the widget should have focus or not.
 */
void sdl_tk_widget_focus(
		struct sdl_tk_widget *widget,
		bool                  focus);

/**
 * Get the title of an sdl-tk widget.
 *
 * \param[in]  widget  The widget to get the title of.
 * \return the title of an sdl-tk widget, or NULL on error.
 */
const char *sdl_tk_widget_get_title(
		struct sdl_tk_widget *widget);

/**
 * Get the detail value for an sdl-tk widget, if any.
 *
 * \param[in]  widget  The widget to get the detail for.
 * \param[in]  size    The text size to get the detail rendered at.
 * \param[in]  col     The text colour to get the detail rendered in.
 * \return an sdl-tk text object for the detail, or NULL on error.
 */
const struct sdl_tk_text *sdl_tk_widget_detail(
		struct sdl_tk_widget *widget,
		sdl_tk_text_size_t    size,
		enum sdl_tk_colour    col);

/**
 * Set a widget to enabled or disabled.
 *
 * \param[in]  widget  The widget to get the detail for.
 * \param[in]  enable  True if the widget should be enabled, false otherwise.
 */
void sdl_tk_widget_enable(
		struct sdl_tk_widget *widget,
		bool                  enable);

#endif /* SDL_TK_WIDGET_H */
