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
 * \brief Implementation of the generic widget handling.
 *
 * This is a generic interface to any kind of widget.
 */

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#include "sdl-tk/colour.h"

#include "sdl-tk/widget.h"
#include "sdl-tk/widget/custom.h"

#include "../text.h"

/* Exported function, documented in include/sdl-tk/widget.h */
void sdl_tk_widget_destroy(
		struct sdl_tk_widget *widget)
{
	if (widget != NULL) {
		free(widget->title);
		assert(widget->t != NULL);
		assert(widget->t->destroy != NULL);
		widget->t->destroy(widget);
	}
}

/* Exported function, documented in include/sdl-tk/widget.h */
void sdl_tk_widget_render(
		struct sdl_tk_widget *widget,
		const SDL_Rect       *rect,
		SDL_Renderer         *ren,
		unsigned              x,
		unsigned              y)
{
	if (widget != NULL) {
		assert(widget->t != NULL);
		if (widget->t->render != NULL) {
			if (widget->focus != SDL_TK_WIDGET_FOCUS_NONE) {
				widget->t->render(widget, rect, ren, x, y);
			}
		}
	}
}

/* Exported function, documented in include/sdl-tk/widget.h */
void sdl_tk_widget_action(
		struct sdl_tk_widget *widget)
{
	if (widget != NULL) {
		if (widget->disabled) {
			return;
		}
		assert(widget->t != NULL);
		if (widget->t->action != NULL) {
			widget->t->action(widget);
		}
	}
}

/* Exported function, documented in include/sdl-tk/widget.h */
void sdl_tk_widget_layout(
		struct sdl_tk_widget *widget)
{
	if (widget != NULL) {
		assert(widget->t != NULL);
		if (widget->t->layout != NULL) {
			widget->t->layout(widget);
		}
	}
}

/* Exported function, documented in include/sdl-tk/widget.h */
bool sdl_tk_widget_input(
		struct sdl_tk_widget *widget,
		SDL_Event         *event)
{
	if (widget != NULL) {
		assert(widget->t != NULL);
		if (widget->t->input != NULL) {
			return widget->t->input(widget, event);
		}
	}

	return false;
}

/* Exported function, documented in include/sdl-tk/widget.h */
void sdl_tk_widget_focus(
		struct sdl_tk_widget *widget,
		bool               focus)
{
	if (widget != NULL) {
		assert(widget->t != NULL);
		if (widget->t->focus != NULL) {
			widget->t->focus(widget, focus);
		}
	}
}

/* Exported function, documented in include/sdl-tk/widget.h */
const char *sdl_tk_widget_get_title(
		struct sdl_tk_widget *widget)
{
	return widget->title;
}

/* Exported function, documented in include/sdl-tk/widget.h */
const struct sdl_tk_text *sdl_tk_widget_detail(
		struct sdl_tk_widget *widget,
		sdl_tk_text_size_t    size,
		enum sdl_tk_colour    col)
{
	if (widget != NULL) {
		assert(widget->t != NULL);
		if (widget->t->detail != NULL) {
			return widget->t->detail(widget, size, col);
		}
	}

	return NULL;
}

/* Exported function, documented in include/sdl-tk/widget.h */
void sdl_tk_widget_enable(
		struct sdl_tk_widget *widget,
		bool                  enable)
{
	if (widget != NULL) {
		widget->disabled = !enable;

		if (widget->parent != NULL) {
			sdl_tk_widget_layout(widget->parent);
		}
	}
}
