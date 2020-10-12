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
 * \brief Implementation of the Select widget.
 *
 * This is a select widget supporting subselects.
 */

#include <assert.h>
#include <stdlib.h>

#include "sdl-tk/widget/select.h"
#include "sdl-tk/widget/custom.h"

#include "../text.h"
#include "../util.h"
#include "../render.h"

/** Select widget entry object. */
struct select_entry {
	struct sdl_tk_text *title[SDL_TK_COLOUR__COUNT];  /**< Name. */
};

/** The sdl-tk select-type widget object. */
struct sdl_tk_widget_select {
	struct sdl_tk_widget  base;  /**< Widget base class. */
	struct sdl_tk_text   *title; /**< Select widget title. */

	sdl_tk_widget_select_cb  cb; /**< Client callback for value changes. */
	void                    *pw; /**< Client private word. */

	struct select_entry *entries; /**< Select widget entries. */
	unsigned             count;   /**< Select widget entry count. */
	unsigned             current; /**< Current select entry. */
};

/**
 * Free the contents of a select entry.
 *
 * \param[in]  entry  The select entry to free the contents of.
 */
static void sdl_tk_widget_select_free_entry_contents(
		struct select_entry *entry)
{
	if (entry != NULL) {
		for (unsigned j = 0; j < SDL_TK_COLOUR__COUNT; j++) {
			sdl_tk_text_destroy(entry->title[j]);
		}
	}
}

/**
 * Destroy an sdl-tk select widget.
 *
 * \param[in]  widget  The widget to destroy.
 */
static void sdl_tk_widget_select_destroy(
		struct sdl_tk_widget *widget)
{
	struct sdl_tk_widget_select *select;

	select = (struct sdl_tk_widget_select *) widget;

	for (unsigned i = 0; i < select->count; i++) {
		sdl_tk_widget_select_free_entry_contents(&select->entries[i]);
	}

	sdl_tk_text_destroy(select->title);
	free(select->entries);
	free(select);
}

/**
 * Set the select widget to a new value.
 *
 * \param[in]  select  The select widget to update.
 * \param[in]  value   The new value to set.
 */
static void set_value(
		struct sdl_tk_widget_select *select,
		unsigned value)
{
	if (value >= select->count) {
		return;
	}

	select->current = value;

	if (select->cb != NULL) {
		select->cb(select->pw, value);
	}

	if (select->base.parent != NULL) {
		sdl_tk_widget_layout(select->base.parent);
	}
}

/**
 * Render an sdl-tk select widget.
 *
 * \param[in]  widget  The widget to render.
 * \param[in]  ren     SDL renderer to use.
 * \param[in]  x       X coordinate.
 * \param[in]  y       Y coordinate.
 */
static void sdl_tk_widget_select_render(
		struct sdl_tk_widget *widget,
		SDL_Renderer         *ren,
		unsigned              x,
		unsigned              y)
{
	SDL_Color background_col = sdl_tk_colour_get(SDL_TK_COLOUR_BACKGROUND);
	SDL_Color interface_col = sdl_tk_colour_get(SDL_TK_COLOUR_INTERFACE);
	SDL_Color selection_col = sdl_tk_colour_get(SDL_TK_COLOUR_SELECTION);
	const struct sdl_tk_widget_select *select;
	SDL_Rect r = {
		.x = x - widget->w / 2,
		.y = y - widget->h / 2,
		.w = widget->w,
		.h = widget->h,
	};

	select = (struct sdl_tk_widget_select *) widget;

	/* Select widget rectangle (title, border) */
	sdl_tk_render_rect(ren, &interface_col, &r);

	/* Title text */
	r.x += EDGE_WIDTH;
	r.y += EDGE_WIDTH;
	r.w = select->title->w;
	r.h = select->title->h;
	SDL_RenderCopy(ren, select->title->t, NULL, &r);
	r.x -= EDGE_WIDTH;
	r.y += EDGE_WIDTH + select->title->h;

	/* Background for select body */
	r.x += BORDER_WIDTH;
	r.w = widget->w - BORDER_WIDTH * 2;
	r.h = widget->h - EDGE_WIDTH * 2 - select->title->h - BORDER_WIDTH;
	sdl_tk_render_rect(ren, &background_col, &r);
	r.y += GUTTER_WIDTH;

	for (unsigned i = 0; i < select->count; i++) {
		const struct select_entry *entry = &select->entries[i];
		const struct sdl_tk_text *title = entry->title[
				select->current == i ?
						SDL_TK_COLOUR_BACKGROUND :
						SDL_TK_COLOUR_INTERFACE];
		unsigned entry_height = title->h;

		r.x += GUTTER_WIDTH;
		r.w = widget->w - (BORDER_WIDTH + GUTTER_WIDTH) * 2;
		r.h = title->h;

		if (select->current == i) {
			/* Selected entry background. */
			sdl_tk_render_rect(ren, &selection_col, &r);
		}
		r.x += PADDING_WIDTH;
		r.w = title->w;

		/* Entry title */
		SDL_RenderCopy(ren, title->t, NULL, &r);

		r.x -= PADDING_WIDTH + GUTTER_WIDTH;
		r.y += entry_height;
	}
}

/**
 * Fire an sdl-tk select widget's action, if any.
 *
 * \param[in]  widget  The widget to activate.
 */
static void sdl_tk_widget_select_action(
		struct sdl_tk_widget *widget)
{
	assert(widget->parent != NULL);
	assert(widget->parent->focus == SDL_TK_WIDGET_FOCUS_TARGET);

	widget->parent->focus = SDL_TK_WIDGET_FOCUS_CHILD;
	widget->focus = SDL_TK_WIDGET_FOCUS_TARGET;
}

/**
 * Navigate the focused select entry up.
 *
 * \param[in]  select  The select widget to navigate.
 */
static void select__nav_up(struct sdl_tk_widget_select *select)
{
	if (select->current == 0) {
		select->current = select->count - 1;
	} else {
		select->current--;
	}

	set_value(select, select->current);
}

/**
 * Navigate the focused select entry down.
 *
 * \param[in]  select  The select widget to navigate.
 */
static void select__nav_down(struct sdl_tk_widget_select *select)
{
	if (select->current == select->count - 1) {
		select->current = 0;
	} else {
		select->current++;
	}

	set_value(select, select->current);
}

/**
 * Layout an sdl-tk select widget.
 *
 * \param[in]  select  The select widget to layout.
 */
static void sdl_tk_widget_select__layout(
		struct sdl_tk_widget_select *select)
{
	unsigned title_max_width = 0;
	unsigned max_width;
	unsigned height;

	assert(select->title != NULL);

	max_width = select->title->w;
	height = select->title->h;

	for (unsigned i = 0; i < select->count; i++) {
		struct select_entry *entry = &select->entries[i];
		const struct sdl_tk_text *title =
				entry->title[SDL_TK_COLOUR_INTERFACE];
		unsigned entry_height;

		if (title == NULL) {
			continue;
		}

		if (title_max_width < title->w) {
			title_max_width = title->w;
		}
		entry_height = title->h;

		height += entry_height;
	}

	if (max_width < title_max_width) {
		max_width = title_max_width;
	}

	select->base.w = EDGE_WIDTH * 2 + max_width;
	select->base.h = EDGE_WIDTH * 2 + height
			+ BORDER_WIDTH + GUTTER_WIDTH * 2;
}

/**
 * Layout an sdl-tk select widget.
 *
 * \param[in]  widget  The widget to layout.
 */
static void sdl_tk_widget_select_layout(
		struct sdl_tk_widget *widget)
{
	struct sdl_tk_widget_select *select;

	select = (struct sdl_tk_widget_select *) widget;

	sdl_tk_widget_select__layout(select);
}

/**
 * Get the detail value for an sdl-tk select widget.
 *
 * \param[in]  widget  The widget to get the detail for.
 * \param[in]  size    The text size to get the detail rendered at.
 * \param[in]  col     The text colour to get the detail rendered in.
 * \return an sdl-tk text object for the detail, or NULL on error.
 */
const struct sdl_tk_text *sdl_tk_widget_select_detail(
		struct sdl_tk_widget *widget,
		sdl_tk_text_size_t    size,
		enum sdl_tk_colour    col)
{
	struct sdl_tk_widget_select *select;

	SDL_TK_UNUSED(size);

	select = (struct sdl_tk_widget_select *) widget;

	assert(select->current < select->count);

	return select->entries[select->current].title[col];
}

/**
 * Handle a keypress.
 *
 * \param[in]  select  Select widget.
 * \param[in]  key   The keypress to handle.
 * \return true if the widget handled the input, false otherwise.
 */
static bool sdl_tk_widget_select_input_keypress(
		struct sdl_tk_widget_select *select,
		SDL_Keycode                  key)
{
	switch (key) {
	case SDLK_UP:
		select__nav_up(select);
		break;

	case SDLK_DOWN:
		select__nav_down(select);
		break;

	case SDLK_RIGHT:  /* Fall though */
	case SDLK_SPACE:  /* Fall though */
	case SDLK_RETURN: /* Fall through */

	case SDLK_LEFT:
		if (select->base.parent != NULL) {
			struct sdl_tk_widget *parent = select->base.parent;
			assert(parent->focus == SDL_TK_WIDGET_FOCUS_CHILD);

			select->base.focus = SDL_TK_WIDGET_FOCUS_NONE;
			parent->focus = SDL_TK_WIDGET_FOCUS_TARGET;
		}
		break;

	default:
		return false;
	}

	return true;
}

/**
 * Fire an input event at an sdl-tk select widget.
 *
 * \param[in]  widget  The widget to fire input at.
 * \param[in]  event   The input event to be handled.
 * \return true if the widget handled the input, false otherwise.
 */
static bool sdl_tk_widget_select_input(
		struct sdl_tk_widget *widget,
		SDL_Event            *event)
{
	struct sdl_tk_widget_select *select;

	select = (struct sdl_tk_widget_select *) widget;

	switch (widget->focus) {
	case SDL_TK_WIDGET_FOCUS_NONE:
		break;

	case SDL_TK_WIDGET_FOCUS_CHILD:
		assert(widget->focus != SDL_TK_WIDGET_FOCUS_CHILD);
		break;

	case SDL_TK_WIDGET_FOCUS_TARGET:
		switch (event->type) {
		case SDL_KEYDOWN:
			return sdl_tk_widget_select_input_keypress(select,
					event->key.keysym.sym);

		case SDL_MOUSEMOTION:
		case SDL_MOUSEBUTTONUP:
		case SDL_MOUSEBUTTONDOWN:
			break;
		}
		break;
	}

	return false;
}

/**
 * Set whether an sdl-tk select widget has input focus.
 *
 * \param[in]  widget  The widget to set focus for.
 * \param[in]  focus   Whether the widget should have focus or not.
 */
static void sdl_tk_widget_select_focus(
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

/** The sdk-tk select-type widget v-table. */
static const struct sdl_tk_widget_vt sdl_tk_widget_select_vt = {
	.destroy = sdl_tk_widget_select_destroy,
	.action  = sdl_tk_widget_select_action,
	.detail  = sdl_tk_widget_select_detail,
	.layout  = sdl_tk_widget_select_layout,
	.render  = sdl_tk_widget_select_render,
	.input   = sdl_tk_widget_select_input,
	.focus   = sdl_tk_widget_select_focus,
};

/* Exported function, documented in include/sdl-tk/widget/select.h */
struct sdl_tk_widget *sdl_tk_widget_select_create(
		struct sdl_tk_widget          *parent,
		const char                    *title,
		const char * const            *options,
		unsigned                       options_count,
		unsigned                       initial,
		const sdl_tk_widget_select_cb  cb,
		void                          *pw)
{
	struct sdl_tk_widget_select *select;

	select = calloc(1, sizeof(*select));
	if (select == NULL) {
		return NULL;
	}
	select->base.t = &sdl_tk_widget_select_vt;
	select->base.parent = parent;
	select->base.title = strdup(title);
	if (select->base.title == NULL) {
		goto error;
	}

	select->title = sdl_tk_text_create(title,
			sdl_tk_colour_get(SDL_TK_COLOUR_BACKGROUND),
			SDL_TK_TEXT_SIZE_NORMAL);
	if (select->title == NULL) {
		goto error;
	}

	select->entries = calloc(options_count, sizeof(*select->entries));
	if (select->entries == NULL) {
		goto error;
	}

	for (unsigned i = 0; i < options_count; i++) {
		for (unsigned j = 0; j < SDL_TK_COLOUR__COUNT; j++) {
			select->entries[i].title[j] =
					sdl_tk_text_create(options[i],
						sdl_tk_colour_get(j),
						SDL_TK_TEXT_SIZE_NORMAL);
			if (select->entries[i].title[j] == NULL) {
				goto error;
			}
		}

		select->count++;
	}

	select->cb = cb;
	select->pw = pw;

	set_value(select, initial);
	sdl_tk_widget_select__layout(select);

	return &select->base;

error:
	sdl_tk_widget_destroy(&select->base);
	return NULL;
}

/* Exported function, documented in include/sdl-tk/widget/select.h */
bool sdl_tk_widget_select_set_value(
		struct sdl_tk_widget *widget,
		unsigned              value)
{
	struct sdl_tk_widget_select *select;

	if (widget->t != &sdl_tk_widget_select_vt) {
		fprintf(stderr, "Error: Called %s with non select widget",
				__func__);
		return false;
	}

	select = (struct sdl_tk_widget_select *) widget;
	set_value(select, value);

	return true;
}
