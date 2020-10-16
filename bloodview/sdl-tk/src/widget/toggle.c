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
 * \brief Implementation of the Toggle widget.
 *
 * This is a toggle widget, i.e. a checkbox.
 */

#include <assert.h>
#include <stdlib.h>

#include "sdl-tk/widget/custom.h"
#include "sdl-tk/widget/toggle.h"

#include "../text.h"
#include "../util.h"

/** The sdl-tk toggle-type widget object. */
struct sdl_tk_widget_toggle {
	struct sdl_tk_widget base;   /**< Widget base class. */

	sdl_tk_widget_toggle_cb  cb; /**< Client callback for value changes. */
	void                    *pw; /**< Client private word. */

	bool value; /**< Current value. */
};

/**
 * Destroy an sdl-tk toggle widget.
 *
 * \param[in]  widget  The toggle widget to destroy.
 */
static void sdl_tk_widget_toggle_destroy(
		struct sdl_tk_widget *widget)
{
	struct sdl_tk_widget_toggle *toggle = (struct sdl_tk_widget_toggle *) widget;

	free(toggle);
}

/**
 * Render an sdl-tk toggle widget.
 *
 * \param[in]  widget  The toggle widget to render.
 * \param[in]  rect    Bounding rectangle for widget placement.
 * \param[in]  ren     SDL renderer to use.
 * \param[in]  x       X-coordinate for widget placement.
 * \param[in]  y       Y-coordinate for widget placement.
 */
static void sdl_tk_widget_toggle_render(
		struct sdl_tk_widget *widget,
		const SDL_Rect       *rect,
		SDL_Renderer         *ren,
		unsigned              x,
		unsigned              y)
{
	SDL_TK_UNUSED(widget);
	SDL_TK_UNUSED(rect);
	SDL_TK_UNUSED(ren);
	SDL_TK_UNUSED(x);
	SDL_TK_UNUSED(y);
}

/**
 * Set the toggle widget to a new value.
 *
 * \param[in]  toggle  The toggle widget to update.
 * \param[in]  value   The new value to set.
 */
static void set_value(
		struct sdl_tk_widget_toggle *toggle,
		bool value)
{
	toggle->value = value;

	if (toggle->cb != NULL) {
		toggle->cb(toggle->pw, value);
	}

	if (toggle->base.parent != NULL) {
		sdl_tk_widget_layout(toggle->base.parent);
	}
}

/**
 * Fire an sdl-tk toggle widget's action, if any.
 *
 * \param[in]  widget  The toggle widget to activate.
 */
static void sdl_tk_widget_toggle_action(
		struct sdl_tk_widget *widget)
{
	struct sdl_tk_widget_toggle *toggle = (struct sdl_tk_widget_toggle *) widget;

	set_value(toggle, !toggle->value);
}

/**
 * Get the detail value for an sdl-tk toggle widget, if any.
 *
 * \param[in]  widget  The toggle widget to get the detail for.
 * \param[in]  size    The text size to get the detail rendered at.
 * \param[in]  col     The text colour to get the detail rendered in.
 * \return an sdl-tk text object for the detail, or NULL on error.
 */
const struct sdl_tk_text *sdl_tk_widget_toggle_detail(
		struct sdl_tk_widget *widget,
		sdl_tk_text_size_t    size,
		enum sdl_tk_colour    col)
{
	struct sdl_tk_widget_toggle *toggle = (struct sdl_tk_widget_toggle *) widget;

	sdl_tk_text_common_t detail = toggle->value ?
			SDL_TK_TEXT_ON : SDL_TK_TEXT_OFF;

	return sdl_tk_text_get_common(col, size, detail);
}

/**
 * Fire an input event at an sdl-tk toggle widget.
 *
 * \param[in]  widget  The toggle widget to fire input at.
 * \param[in]  event   The input event to be handled.
 * \param[in]  rect    Bounding rectangle for widget placement.
 * \param[in]  x       X-coordinate for widget placement.
 * \param[in]  y       Y-coordinate for widget placement.
 * \return true if the widget handled the input, false otherwise.
 */
static bool sdl_tk_widget_toggle_input(
		struct sdl_tk_widget *widget,
		SDL_Event            *event,
		const SDL_Rect       *rect,
		unsigned              x,
		unsigned              y)
{
	SDL_TK_UNUSED(widget);
	SDL_TK_UNUSED(event);
	SDL_TK_UNUSED(rect);
	SDL_TK_UNUSED(x);
	SDL_TK_UNUSED(y);

	return true;
}

/** The sdk-tk toggle-type widget v-table. */
static const struct sdl_tk_widget_vt sdl_tk_widget_toggle_vt = {
	.destroy = sdl_tk_widget_toggle_destroy,
	.action  = sdl_tk_widget_toggle_action,
	.detail  = sdl_tk_widget_toggle_detail,
	.render  = sdl_tk_widget_toggle_render,
	.input   = sdl_tk_widget_toggle_input,
};

/* Exported function, documented in include/sdl-tk/widget/toggle.h */
struct sdl_tk_widget *sdl_tk_widget_toggle_create(
		struct sdl_tk_widget          *parent,
		const char                    *title,
		bool                           initial,
		const sdl_tk_widget_toggle_cb  cb,
		void                          *pw)
{
	struct sdl_tk_widget_toggle *toggle;

	toggle = calloc(1, sizeof(*toggle));
	if (toggle == NULL) {
		return NULL;
	}
	toggle->base.t = &sdl_tk_widget_toggle_vt;
	toggle->base.parent = parent;
	toggle->base.title = strdup(title);
	if (toggle->base.title == NULL) {
		goto error;
	}

	toggle->cb = cb;
	toggle->pw = pw;

	set_value(toggle, initial);

	return &toggle->base;

error:
	sdl_tk_widget_destroy(&toggle->base);
	return NULL;
}

/* Exported function, documented in include/sdl-tk/widget/toggle.h */
bool sdl_tk_widget_toggle_set_value(
		struct sdl_tk_widget *widget,
		bool value)
{
	struct sdl_tk_widget_toggle *toggle;

	if (widget->t != &sdl_tk_widget_toggle_vt) {
		fprintf(stderr, "Error: Called %s with non toggle widget",
				__func__);
		return false;
	}

	toggle = (struct sdl_tk_widget_toggle *) widget;
	set_value(toggle, value);

	return true;
}
