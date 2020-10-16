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
 * \brief Implementation of the Action widget.
 *
 * This is an action widget, i.e. a button.
 */

#include <assert.h>
#include <stdlib.h>

#include "sdl-tk/widget/action.h"
#include "sdl-tk/widget/custom.h"

#include "../text.h"
#include "../util.h"

/** The sdl-tk action-type widget object. */
struct sdl_tk_widget_action {
	struct sdl_tk_widget base;  /**< Widget base class. */

	sdl_tk_widget_action_cb cb; /**< Callback for performaing action. */
	void *pw;                   /**< Client private word. */
};

/**
 * Destroy an sdl-tk action widget.
 *
 * \param[in]  widget  The widget to destroy.
 */
static void sdl_tk_widget_action_destroy(
		struct sdl_tk_widget *widget)
{
	struct sdl_tk_widget_action *action = (struct sdl_tk_widget_action *) widget;

	free(action);
}

/**
 * Render an sdl-tk action widget.
 *
 * \param[in]  widget  The widget to render.
 * \param[in]  ren     SDL renderer to use.
 * \param[in]  rect    Bounding rectangle for widget placement.
 * \param[in]  x       X-coordinate for widget placement.
 * \param[in]  y       Y-coordinate for widget placement.
 */
static void sdl_tk_widget_action_render(
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
 * Fire an sdl-tk action widget's action, if any.
 *
 * \param[in]  widget  The widget to activate.
 */
static void sdl_tk_widget_action_action(
		struct sdl_tk_widget *widget)
{
	struct sdl_tk_widget_action *action = (struct sdl_tk_widget_action *) widget;

	if (action->cb != NULL) {
		action->cb(action->pw);
	}
}

/**
 * Fire an input event at an sdl-tk action widget.
 *
 * \param[in]  widget  The widget to fire input at.
 * \param[in]  event   The input event to be handled.
 * \param[in]  rect    Bounding rectangle for widget placement.
 * \param[in]  x       X-coordinate for widget placement.
 * \param[in]  y       Y-coordinate for widget placement.
 * \return true if the widget handled the input, false otherwise.
 */
static bool sdl_tk_widget_action_input(
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

/** The sdk-tk action-type widget v-table. */
static const struct sdl_tk_widget_vt sdl_tk_widget_action_vt = {
	.destroy = sdl_tk_widget_action_destroy,
	.action  = sdl_tk_widget_action_action,
	.render  = sdl_tk_widget_action_render,
	.input   = sdl_tk_widget_action_input,
};

/* Exported function, documented in include/sdl-tk/widget/action.h */
struct sdl_tk_widget *sdl_tk_widget_action_create(
		struct sdl_tk_widget          *parent,
		const char                    *title,
		const sdl_tk_widget_action_cb  cb,
		void                          *pw)
{
	struct sdl_tk_widget_action *action;

	action = calloc(1, sizeof(*action));
	if (action == NULL) {
		return NULL;
	}
	action->base.t = &sdl_tk_widget_action_vt;
	action->base.parent = parent;
	action->base.title = strdup(title);
	if (action->base.title == NULL) {
		goto error;
	}

	action->cb = cb;
	action->pw = pw;

	return &action->base;

error:
	sdl_tk_widget_destroy(&action->base);
	return NULL;
}
