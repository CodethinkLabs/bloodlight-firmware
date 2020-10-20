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
 * \brief Implementation of the Input widget.
 *
 * This is a single-line text entry widget.
 */

#include <time.h>
#include <assert.h>
#include <stdlib.h>

#include "sdl-tk/widget/input.h"
#include "sdl-tk/widget/custom.h"

#include "../text.h"
#include "../util.h"
#include "../render.h"

/** The sdl-tk input-type widget object. */
struct sdl_tk_widget_input {
	struct sdl_tk_widget  base;  /**< Widget base class. */
	struct sdl_tk_text   *title; /**< Widget title. */

	sdl_tk_widget_input_cb  cb; /**< Client update callback. */
	void                   *pw; /**< Client private word. */

	/** Text to render to represent widget value. */
	struct sdl_tk_text *detail[SDL_TK_COLOUR__COUNT];
	char               *value; /**< The current widget value. */

	bool     cursor_show; /**< Whether to render the cursor. */
	time_t   cursor_time; /**< Previous cursor render time in seconds. */
	unsigned cursor_x;    /**< Cursor position in pixels. */
};

/**
 * Helper to get input detail height.
 *
 * \param[in] input  Text input widget.
 * \return the detail height in pixels.
 */
static inline unsigned input__get_detail_height(
		const struct sdl_tk_widget_input *input)
{
	struct sdl_tk_text *detail = input->detail[SDL_TK_COLOUR_INTERFACE];
	unsigned height = input->title->h;

	if (detail != NULL) {
		if (detail->h > input->title->h) {
			height = detail->h;
		}
	}

	return height;
}

/**
 * Lay out an input widget.
 *
 * \param[in]  input  The input widget to lay out.
 */
static void sdl_tk_widget_input__layout(
		struct sdl_tk_widget_input *input)
{
	struct sdl_tk_text *detail = input->detail[SDL_TK_COLOUR_INTERFACE];
	unsigned max_width;
	unsigned cursor_x;
	unsigned height;

	assert(input->title != NULL);

	max_width = input->title->w;
	height = input->title->h;
	cursor_x = 0;

	if (detail != NULL) {
		if (max_width < detail->w) {
			max_width = detail->w;
		}
		cursor_x = detail->w;
	}
	height += input__get_detail_height(input);

	input->base.w = EDGE_WIDTH * 2 + max_width;
	input->base.h = EDGE_WIDTH * 4 + height;

	input->cursor_x = cursor_x;
}

/**
 * Layout an sdl-tk input widget.
 *
 * \param[in]  widget  The widget to layout.
 */
static void sdl_tk_widget_input_layout(
		struct sdl_tk_widget *widget)
{
	struct sdl_tk_widget_input *input = (struct sdl_tk_widget_input *) widget;

	sdl_tk_widget_input__layout(input);
}

/**
 * Update the detail rendered value textures.
 *
 * \param[in]  input  The input widget update detail for.
 * \return true on success, or false on failure.
 */
static bool sdl_tk_widget_input__update_detail(
		struct sdl_tk_widget_input *input)
{
	for (unsigned i = 0; i < SDL_TK_COLOUR__COUNT; i++) {
		sdl_tk_text_destroy(input->detail[i]);
		input->detail[i] = NULL;

		if (input->value == NULL || strlen(input->value) == 0) {
			continue;
		}

		input->detail[i] = sdl_tk_text_create(input->value,
				sdl_tk_colour_get(i),
				SDL_TK_TEXT_SIZE_NORMAL);
		if (input->detail[i] == NULL) {
			return false;
		}
	}

	sdl_tk_widget_input__layout(input);

	return true;
}

/**
 * Destroy an sdl-tk input widget.
 *
 * \param[in]  widget  The widget to destroy.
 */
static void sdl_tk_widget_input_destroy(
		struct sdl_tk_widget *widget)
{
	struct sdl_tk_widget_input *input = (struct sdl_tk_widget_input *) widget;

	for (unsigned i = 0; i < SDL_TK_COLOUR__COUNT; i++) {
		sdl_tk_text_destroy(input->detail[i]);
	}

	sdl_tk_text_destroy(input->title);
	free(input->value);
	free(input);
}

/**
 * Get the detail value for an sdl-tk input widget, if any.
 *
 * \param[in]  widget  The input widget to get the detail for.
 * \param[in]  size    The text size to get the detail rendered at.
 * \param[in]  col     The text colour to get the detail rendered in.
 * \return an sdl-tk text object for the detail, or NULL on error.
 */
const struct sdl_tk_text *sdl_tk_widget_input_detail(
		struct sdl_tk_widget *widget,
		sdl_tk_text_size_t    size,
		enum sdl_tk_colour    col)
{
	struct sdl_tk_widget_input *input = (struct sdl_tk_widget_input *) widget;

	SDL_TK_UNUSED(size);

	return input->detail[col];
}

/**
 * Render an sdl-tk input widget.
 *
 * \param[in]  widget  The input widget to render.
 * \param[in]  rect    Bounding rectangle for widget placement.
 * \param[in]  ren     SDL renderer to use.
 * \param[in]  x       X-coordinate for widget placement.
 * \param[in]  y       Y-coordinate for widget placement.
 */
static void sdl_tk_widget_input_render(
		struct sdl_tk_widget *widget,
		const SDL_Rect       *rect,
		SDL_Renderer         *ren,
		unsigned              x,
		unsigned              y)
{
	struct sdl_tk_widget_input *input = (struct sdl_tk_widget_input *) widget;
	SDL_Color background_col = sdl_tk_colour_get(SDL_TK_COLOUR_BACKGROUND);
	SDL_Color interface_col = sdl_tk_colour_get(SDL_TK_COLOUR_INTERFACE);
	unsigned h = input__get_detail_height(input);
	time_t now = time(NULL);
	SDL_Rect r = {
		.x = x - widget->w / 2,
		.y = y - widget->h / 2,
		.w = widget->w,
		.h = widget->h,
	};

	if (now >= 0 && now != input->cursor_time) {
		input->cursor_show = !input->cursor_show;
		input->cursor_time = now;
	}

	sdl_tl__shift_rect(rect, &r);

	/* Input rectangle (title, border) */
	sdl_tk_render_rect(ren, &interface_col, &r);

	/* Title text */
	r.x += EDGE_WIDTH;
	r.y += EDGE_WIDTH;
	r.w = input->title->w;
	r.h = input->title->h;
	SDL_RenderCopy(ren, input->title->t, NULL, &r);
	r.x -= EDGE_WIDTH;
	r.y += EDGE_WIDTH + input->title->h;

	/* Background for input body */
	r.x += BORDER_WIDTH;
	r.w = widget->w - BORDER_WIDTH * 2;
	r.h = widget->h - EDGE_WIDTH * 2 - input->title->h - BORDER_WIDTH;
	sdl_tk_render_rect(ren, &background_col, &r);
	r.x -= BORDER_WIDTH;

	if (input->detail[SDL_TK_COLOUR_INTERFACE] != NULL) {
		r.x += EDGE_WIDTH;
		r.y += EDGE_WIDTH;
		r.w = input->detail[SDL_TK_COLOUR_INTERFACE]->w;
		r.h = input->detail[SDL_TK_COLOUR_INTERFACE]->h;
		SDL_RenderCopy(ren, input->detail[SDL_TK_COLOUR_INTERFACE]->t,
				NULL, &r);
		r.x -= EDGE_WIDTH;
		r.y -= EDGE_WIDTH;
	}

	if (input->cursor_show) {
		if (widget->focus == SDL_TK_WIDGET_FOCUS_TARGET) {
			r.x += EDGE_WIDTH + input->cursor_x;
			r.y += EDGE_WIDTH;
			SDL_SetRenderDrawColor(ren,
					interface_col.r,
					interface_col.g,
					interface_col.b,
					SDL_ALPHA_OPAQUE);
			SDL_RenderDrawLine(ren, r.x, r.y, r.x, r.y + h);
		}
	}
}

/**
 * Force the cursor flash pattern back to showing.
 *
 * \param[in]  input  A text input widget.
 */
static void input__cursor_flash_reset(struct sdl_tk_widget_input *input)
{
	input->cursor_show = true;
	input->cursor_time = time(NULL);
}

/**
 * Fire an sdl-tk input widget's action, if any.
 *
 * \param[in]  widget  The input widget to activate.
 */
static void sdl_tk_widget_input_action(
		struct sdl_tk_widget *widget)
{
	struct sdl_tk_widget_input *input = (struct sdl_tk_widget_input *) widget;

	assert(widget->parent != NULL);
	assert(widget->parent->focus == SDL_TK_WIDGET_FOCUS_TARGET);

	widget->parent->focus = SDL_TK_WIDGET_FOCUS_CHILD;
	widget->focus = SDL_TK_WIDGET_FOCUS_TARGET;

	input__cursor_flash_reset(input);
}

/**
 * Update an input widget's value.
 *
 * \param[in]  input      Input widget to update value of.
 * \param[in]  new_value  New value to set.  Ownership is passed in.
 * \return true if the widget value changed, false otherwise.
 */
static bool update_value(
		struct sdl_tk_widget_input *input,
		char *new_value)
{
	bool updated = false;

	if (new_value == NULL) {
		return false;
	}

	if ((input->cb != NULL && input->cb(input->pw, new_value)) ||
	    (input->cb == NULL)) {
		free(input->value);
		input->value = new_value;

		sdl_tk_widget_input__update_detail(input);
		sdl_tk_widget_input__layout(input);

		updated = true;
	} else {
		free(new_value);
	}

	input__cursor_flash_reset(input);

	return updated;
}

/**
 * Append a character to an input widget's value.
 *
 * \param[in]  input    Input widget to update value for.
 * \param[in]  c        Character to add to input widget's value.
 * \param[out] updated  Returns whether the input widget's value was changed.
 * \return true on success, or false on error.
 */
static bool append_char(
		struct sdl_tk_widget_input *input,
		char c,
		bool *updated)
{
	size_t len = strlen(input->value);
	char *new = strdup(input->value);
	char *temp;

	if (new == NULL) {
		return false;
	}

	temp = realloc(new, len + 2);
	if (temp == NULL) {
		free(new);
		return false;
	}
	new = temp;
	new[len] = c;
	new[len + 1] = '\0';

	*updated = update_value(input, new);

	return true;
}

/**
 * Handle keypresses in the input widget.
 *
 * TODO: This is very primative at the moment.  It could do with:
 *       - Cursor display
 *       - Tracking and handling of modifiers, e.g. shift.
 *
 * \param[in]  input  Input widget.
 * \param[in]  key    The keypress to handle.
 */
static bool sdl_tk_widget_input_input_keypress(
		struct sdl_tk_widget_input *input,
		SDL_Keycode                 key)
{
	bool handled = true;
	bool change = false;

	switch (key) {
	case SDLK_BACKSPACE:
	{
		size_t len = strlen(input->value);
		if (len > 0) {
			input->value[len - 1] = '\0';

			sdl_tk_widget_input__update_detail(input);
			sdl_tk_widget_input__layout(input);
			input__cursor_flash_reset(input);

			change = true;
		}
		break;
	}

	case SDLK_RETURN:
		if (input->base.parent != NULL) {
			struct sdl_tk_widget *parent = input->base.parent;
			assert(parent->focus == SDL_TK_WIDGET_FOCUS_CHILD);

			input->base.focus = SDL_TK_WIDGET_FOCUS_NONE;
			parent->focus = SDL_TK_WIDGET_FOCUS_TARGET;
		}
		break;

	case SDLK_KP_0: handled = append_char(input, '0', &change); break;
	case SDLK_KP_1: handled = append_char(input, '1', &change); break;
	case SDLK_KP_2: handled = append_char(input, '2', &change); break;
	case SDLK_KP_3: handled = append_char(input, '3', &change); break;
	case SDLK_KP_4: handled = append_char(input, '4', &change); break;
	case SDLK_KP_5: handled = append_char(input, '5', &change); break;
	case SDLK_KP_6: handled = append_char(input, '6', &change); break;
	case SDLK_KP_7: handled = append_char(input, '7', &change); break;
	case SDLK_KP_8: handled = append_char(input, '8', &change); break;
	case SDLK_KP_9: handled = append_char(input, '9', &change); break;

	default:
		if (key >= ' ' && key <= 'z') {
			handled = append_char(input, (char) key, &change);
		} else {
			handled = false;
		}

		break;
	}

	if (change) {
		if (input->base.parent != NULL) {
			sdl_tk_widget_layout(input->base.parent);
		}
	}

	return handled;
}

/**
 * Fire an input event at an sdl-tk menu widget.
 *
 * \param[in]  input   Input widget.
 * \param[in]  event   The input event to be handled.
 * \param[in]  rect    Bounding rectangle for widget placement.
 * \param[in]  x       X-coordinate for widget placement.
 * \param[in]  y       Y-coordinate for widget placement.
 * \return true if the widget handled the input, false otherwise.
 */
static bool sdl_tk_widget_input_input_mouse(
		struct sdl_tk_widget_input *input,
		SDL_Event                  *event,
		const SDL_Rect             *rect,
		unsigned                    x,
		unsigned                    y)
{
	bool handled = false;
	SDL_Rect r = {
		.x = x - input->base.w / 2,
		.y = y - input->base.h / 2,
		.w = input->base.w,
		.h = input->base.h,
	};
	int pos_x;
	int pos_y;

	sdl_tl__shift_rect(rect, &r);
	SDL_GetMouseState(&pos_x, &pos_y);

	if (pos_x < r.x || pos_x >= r.x + r.w ||
	    pos_y < r.y || pos_y >= r.y + r.h) {
		/* Outside widget area. */
		goto cleanup;
	}
	handled = true;

	/* TODO: Handle the event. */
	SDL_TK_UNUSED(event);

cleanup:
	return handled;
}

/**
 * Fire an input event at an sdl-tk input widget.
 *
 * \param[in]  widget  The input widget to fire input at.
 * \param[in]  event   The input event to be handled.
 * \param[in]  rect    Bounding rectangle for widget placement.
 * \param[in]  x       X-coordinate for widget placement.
 * \param[in]  y       Y-coordinate for widget placement.
 * \return true if the widget handled the input, false otherwise.
 */
static bool sdl_tk_widget_input_input(
		struct sdl_tk_widget *widget,
		SDL_Event            *event,
		const SDL_Rect       *rect,
		unsigned              x,
		unsigned              y)
{
	struct sdl_tk_widget_input *input = (struct sdl_tk_widget_input *) widget;

	switch (widget->focus) {
	case SDL_TK_WIDGET_FOCUS_NONE:
		break;

	case SDL_TK_WIDGET_FOCUS_CHILD:
		assert(widget->focus != SDL_TK_WIDGET_FOCUS_CHILD);
		break;

	case SDL_TK_WIDGET_FOCUS_TARGET:
		switch (event->type) {
		case SDL_KEYDOWN:
			return sdl_tk_widget_input_input_keypress(input,
					event->key.keysym.sym);

		case SDL_MOUSEMOTION:
		case SDL_MOUSEBUTTONUP:
		case SDL_MOUSEBUTTONDOWN:
			return sdl_tk_widget_input_input_mouse(input,
					event, rect, x, y);
		}
		break;
	}

	return false;
}

/**
 * Set whether an sdl-tk input widget has input focus.
 *
 * \param[in]  widget  The input widget to set focus for.
 * \param[in]  focus   Whether the widget should have focus or not.
 */
static void sdl_tk_widget_input_focus(
		struct sdl_tk_widget *widget,
		bool                  focus)
{
	if (widget->focus == SDL_TK_WIDGET_FOCUS_NONE ||
	    widget->focus == SDL_TK_WIDGET_FOCUS_TARGET) {
		widget->focus = (focus) ?
				SDL_TK_WIDGET_FOCUS_TARGET :
				SDL_TK_WIDGET_FOCUS_NONE;
	}

	if (widget->parent != NULL) {
		assert(widget->parent->focus == SDL_TK_WIDGET_FOCUS_CHILD);
		widget->parent->focus = SDL_TK_WIDGET_FOCUS_TARGET;
	}
}

/** The sdk-tk input-type widget v-table. */
static const struct sdl_tk_widget_vt sdl_tk_widget_input_vt = {
	.destroy = sdl_tk_widget_input_destroy,
	.action  = sdl_tk_widget_input_action,
	.detail  = sdl_tk_widget_input_detail,
	.layout  = sdl_tk_widget_input_layout,
	.render  = sdl_tk_widget_input_render,
	.input   = sdl_tk_widget_input_input,
	.focus   = sdl_tk_widget_input_focus,
};

/* Exported function, documented in include/sdl-tk/widget/input.h */
struct sdl_tk_widget *sdl_tk_widget_input_create(
		struct sdl_tk_widget         *parent,
		const char                   *title,
		const char                   *initial,
		const sdl_tk_widget_input_cb  cb,
		void                         *pw)
{
	struct sdl_tk_widget_input *input;
	char *value;

	input = calloc(1, sizeof(*input));
	if (input == NULL) {
		return NULL;
	}
	input->base.t = &sdl_tk_widget_input_vt;
	input->base.parent = parent;
	input->base.title = strdup(title);
	if (input->base.title == NULL) {
		goto error;
	}

	input->title = sdl_tk_text_create(title,
			sdl_tk_colour_get(SDL_TK_COLOUR_BACKGROUND),
			SDL_TK_TEXT_SIZE_NORMAL);
	if (input->title == NULL) {
		goto error;
	}

	input->cb = cb;
	input->pw = pw;

	value = strdup((initial != NULL) ? initial : "");
	if (value == NULL) {
		goto error;
	}

	if (!update_value(input, value)) {
		goto error;
	}

	if (!sdl_tk_widget_input__update_detail(input)) {
		goto error;
	}

	return &input->base;

error:
	sdl_tk_widget_destroy(&input->base);
	return NULL;
}

/* Exported function, documented in include/sdl-tk/widget/input.h */
bool sdl_tk_widget_input_set_value(
			struct sdl_tk_widget *widget,
			const char *value)
{
	struct sdl_tk_widget_input *input;
	bool updated;

	if (widget->t != &sdl_tk_widget_input_vt) {
		fprintf(stderr, "Error: Called %s with non input widget",
				__func__);
		return false;
	}

	input = (struct sdl_tk_widget_input *) widget;
	updated = update_value(input, strdup(value));

	if (updated) {
		if (input->base.parent != NULL) {
			sdl_tk_widget_layout(input->base.parent);
		}
	}

	return updated;
}
