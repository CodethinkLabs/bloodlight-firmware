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

#ifndef SDL_TK_WIDGET_CUSTOM_H
#define SDL_TK_WIDGET_CUSTOM_H

#include "sdl-tk/widget.h"

/**
 * \file
 * \brief Header for implementing new widgets.
 *
 * This only needs to be incuded where you're creating a new widget type.
 */

/**
 * Types of input focus for an sdl-tk widget.
 */
enum sdl_tk_widget_focus {
	SDL_TK_WIDGET_FOCUS_NONE,
	SDL_TK_WIDGET_FOCUS_CHILD,
	SDL_TK_WIDGET_FOCUS_TARGET,
};

/**
 * Common base class of an sdl-tk widget.
 */
struct sdl_tk_widget {
	struct sdl_tk_widget     *parent;
	enum sdl_tk_widget_focus  focus;

	char *title;
	bool  disabled;

	unsigned w;
	unsigned h;

	const struct sdl_tk_widget_vt *t;
};

/** The sdl-tk widget interface v-table. */
struct sdl_tk_widget_vt {
	/**
	 * Destroy an sdl-tk widget.
	 *
	 * \param[in]  widget  The widget to destroy.
	 */
	void (*destroy)(
			struct sdl_tk_widget *widget);

	/**
	 * Render an sdl-tk widget.
	 *
	 * \param[in]  widget  The widget to render.
	 * \param[in]  ren     SDL renderer to use.
	 * \param[in]  x       X coordinate.
	 * \param[in]  y       Y coordinate.
	 */
	void (*render)(
			struct sdl_tk_widget *widget,
			SDL_Renderer         *ren,
			unsigned              x,
			unsigned              y);

	/**
	 * Fire an sdl-tk widget's action, if any.
	 *
	 * \param[in]  widget  The widget to activate.
	 */
	void (*action)(
			struct sdl_tk_widget *widget);

	/**
	 * Layout an sdl-tk widget.
	 *
	 * \param[in]  widget  The widget to layout.
	 */
	void (*layout)(
			struct sdl_tk_widget *widget);

	/**
	 * Fire an input event at an sdl-tk widget.
	 *
	 * \param[in]  widget  The widget to fire input at.
	 * \param[in]  event   The input event to be handled.
	 * \return true if the widget handled the input, false otherwise.
	 */
	bool (*input)(
			struct sdl_tk_widget *widget,
			SDL_Event            *event);

	/**
	 * Set whether an sdl-tk widget has input focus.
	 *
	 * \param[in]  widget  The widget to set focus for.
	 * \param[in]  focus   Whether the widget should have focus or not.
	 */
	void (*focus)(
			struct sdl_tk_widget *widget,
			bool                  focus);

	/**
	 * Get the detail value for an sdl-tk widget, if any.
	 *
	 * \param[in]  widget  The widget to get the detail for.
	 * \param[in]  size    The text size to get the detail rendered at.
	 * \param[in]  col     The text colour to get the detail rendered in.
	 * \return an sdl-tk text object for the detail, or NULL on error.
	 */
	const struct sdl_tk_text *(*detail)(
			struct sdl_tk_widget *widget,
			sdl_tk_text_size_t    size,
			enum sdl_tk_colour    col);
};

#define BORDER_WIDTH  2
#define GUTTER_WIDTH  2
#define PADDING_WIDTH 2
#define EDGE_WIDTH    (BORDER_WIDTH + GUTTER_WIDTH + PADDING_WIDTH)

#endif /* SDL_TK_WIDGET_CUSTOM_H */
