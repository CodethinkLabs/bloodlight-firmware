
#include <assert.h>
#include <stdlib.h>

#include "sdl-tk/widget/menu.h"
#include "sdl-tk/widget/custom.h"

#include "../text.h"
#include "../util.h"
#include "../render.h"

/** Menu entry state. */
enum sdl_tk_widget_state {
	SDL_TK_WIDGET_NORMAL,
	SDL_TK_WIDGET_SELECTED,
	SDL_TK_WIDGET_DISABLED,
	SDL_TK_WIDGET__COUNT,
};

/** Menu entry object. */
struct sdl_tk_widget_menu_entry {
	struct sdl_tk_text       *title[SDL_TK_WIDGET__COUNT];
	const struct sdl_tk_text *detail[SDL_TK_WIDGET__COUNT];
	struct sdl_tk_widget     *widget;
};

/** The sdl-tk menu-type widget object. */
struct sdl_tk_widget_menu {
	struct sdl_tk_widget  base;
	struct sdl_tk_text   *title;

	struct sdl_tk_widget_menu_entry *entries;
	unsigned                         count;
	unsigned                         current;
};

/**
 * Destroy an sdl-tk menu widget.
 *
 * \param[in]  widget  The widget to destroy.
 */
static void sdl_tk_widget_menu_destroy(
		struct sdl_tk_widget *widget)
{
	struct sdl_tk_widget_menu *menu = (struct sdl_tk_widget_menu *) widget;

	for (unsigned i = 0; i < menu->count; i++) {
		struct sdl_tk_widget_menu_entry *entry = &menu->entries[i];

		sdl_tk_text_destroy(entry->title[SDL_TK_WIDGET_DISABLED]);
		sdl_tk_text_destroy(entry->title[SDL_TK_WIDGET_SELECTED]);
		sdl_tk_text_destroy(entry->title[SDL_TK_WIDGET_NORMAL]);
		sdl_tk_widget_destroy(entry->widget);
	}

	sdl_tk_text_destroy(menu->title);
	free(menu->entries);
	free(menu);
}

static enum sdl_tk_widget_state sdl_tk_entry_state(
		const struct sdl_tk_widget_menu *menu,
		const struct sdl_tk_widget_menu_entry *entry)
{
	if ((entry - menu->entries) == menu->current) {
		return SDL_TK_WIDGET_SELECTED;
	}
	if (entry->widget->disabled) {
		return SDL_TK_WIDGET_DISABLED;
	}
	return SDL_TK_WIDGET_NORMAL;
}

/**
 * Render an sdl-tk menu widget.
 *
 * \param[in]  widget  The widget to render.
 * \param[in]  ren     SDL renderer to use.
 * \param[in]  x       X coordinate.
 * \param[in]  y       Y coordinate.
 */
static void sdl_tk_widget_menu_render(
		struct sdl_tk_widget *widget,
		SDL_Renderer         *ren,
		unsigned              x,
		unsigned              y)
{
	const struct sdl_tk_widget_menu *menu = (struct sdl_tk_widget_menu *) widget;
	SDL_Color background_col = sdl_tk_colour_get(SDL_TK_COLOUR_BACKGROUND);
	SDL_Color interface_col = sdl_tk_colour_get(SDL_TK_COLOUR_INTERFACE);
	SDL_Color selection_col = sdl_tk_colour_get(SDL_TK_COLOUR_SELECTION);
	SDL_Color sel_dis_col = sdl_tk_colour_get(SDL_TK_COLOUR_SEL_DIS);
	SDL_Rect r = {
		.x = x - widget->w / 2,
		.y = y - widget->h / 2,
		.w = widget->w,
		.h = widget->h,
	};

	if (widget->focus == SDL_TK_WIDGET_FOCUS_CHILD) {
		sdl_tk_widget_render(menu->entries[menu->current].widget,
				ren, x, y);
		return;
	}

	/* Menu rectangle (title, border) */
	sdl_tk_render_rect(ren, &interface_col, &r);

	/* Title text */
	r.x += EDGE_WIDTH;
	r.y += EDGE_WIDTH;
	r.w = menu->title->w;
	r.h = menu->title->h;
	SDL_RenderCopy(ren, menu->title->t, NULL, &r);
	r.x -= EDGE_WIDTH;
	r.y += EDGE_WIDTH + menu->title->h;

	/* Background for menu body */
	r.x += BORDER_WIDTH;
	r.w = widget->w - BORDER_WIDTH * 2;
	r.h = widget->h - EDGE_WIDTH * 2 - menu->title->h - BORDER_WIDTH;
	sdl_tk_render_rect(ren, &background_col, &r);
	r.y += GUTTER_WIDTH;

	for (unsigned i = 0; i < menu->count; i++) {
		const struct sdl_tk_widget_menu_entry *entry = &menu->entries[i];
		const struct sdl_tk_text *title = entry->title[
				sdl_tk_entry_state(menu, entry)];
		const struct sdl_tk_text *detail = entry->detail[
				sdl_tk_entry_state(menu, entry)];
		unsigned entry_height = title->h;

		r.x += GUTTER_WIDTH;
		r.w = widget->w - (BORDER_WIDTH + GUTTER_WIDTH) * 2;
		r.h = title->h;

		if (menu->current == i) {
			/* Selected entry background. */
			sdl_tk_render_rect(ren, entry->widget->disabled ?
					&sel_dis_col : &selection_col, &r);
		}
		r.x += PADDING_WIDTH;
		r.w = title->w;

		/* Entry title */
		SDL_RenderCopy(ren, title->t, NULL, &r);

		if (detail != NULL) {
			unsigned detail_offset = widget->w
					- EDGE_WIDTH * 2
					- detail->w;
			r.x += detail_offset;
			r.w = detail->w;
			r.h = detail->h;
			SDL_RenderCopy(ren, detail->t, NULL, &r);
			r.x -= detail_offset;

			if (entry_height < detail->h) {
				entry_height = detail->h;
			}
		}
		r.x -= PADDING_WIDTH + GUTTER_WIDTH;
		r.y += entry_height;
	}
}

/**
 * Fire an sdl-tk menu widget's action, if any.
 *
 * \param[in]  widget  The widget to activate.
 */
static void sdl_tk_widget_menu_action(
		struct sdl_tk_widget *widget)
{
	assert(widget->parent != NULL);
	assert(widget->parent->focus == SDL_TK_WIDGET_FOCUS_TARGET);

	widget->parent->focus = SDL_TK_WIDGET_FOCUS_CHILD;
	widget->focus = SDL_TK_WIDGET_FOCUS_TARGET;
}

/**
 * Navigate the focused menu entry up.
 *
 * \param[in]  menu  The menu widget to navigate.
 */
static void menu__nav_up_internal(struct sdl_tk_widget_menu *menu)
{
	if (menu->current == 0) {
		menu->current = menu->count - 1;
	} else {
		menu->current--;
	}
}

/**
 * Navigate the focused menu entry down.
 *
 * \param[in]  menu  The menu widget to navigate.
 */
static void menu__nav_down_internal(struct sdl_tk_widget_menu *menu)
{
	if (menu->current == menu->count - 1) {
		menu->current = 0;
	} else {
		menu->current++;
	}
}

/**
 * Navigate the focused menu entry up.
 *
 * \param[in]  menu  The menu widget to navigate.
 */
static void menu__nav_up(struct sdl_tk_widget_menu *menu)
{
	unsigned current = menu->current;

	do {
		menu__nav_up_internal(menu);
	} while (menu->entries[menu->current].widget->disabled &&
		 menu->current != current);
}

/**
 * Navigate the focused menu entry down.
 *
 * \param[in]  menu  The menu widget to navigate.
 */
static void menu__nav_down(struct sdl_tk_widget_menu *menu)
{
	unsigned current = menu->current;

	do {
		menu__nav_down_internal(menu);
	} while (menu->entries[menu->current].widget->disabled &&
		 menu->current != current);
}

/**
 * Layout an sdl-tk menu widget.
 *
 * \param[in]  menu  The menu widget to layout.
 */
static void sdl_tk_widget_menu__layout(
		struct sdl_tk_widget_menu *menu)
{
	unsigned detail_max_width = 0;
	unsigned title_max_width = 0;
	unsigned max_width;
	unsigned height;

	assert(menu->title != NULL);

	max_width = menu->title->w;
	height = menu->title->h;

	if (menu->entries[menu->current].widget->disabled) {
		menu__nav_down(menu);
	}

	for (unsigned i = 0; i < menu->count; i++) {
		struct sdl_tk_widget_menu_entry *entry = &menu->entries[i];
		const struct sdl_tk_text *title = entry->title[SDL_TK_WIDGET_NORMAL];
		const struct sdl_tk_text *detail;
		unsigned entry_height;

		if (title == NULL) {
			continue;
		}

		entry->detail[SDL_TK_WIDGET_NORMAL] = sdl_tk_widget_detail(
				entry->widget,
				SDL_TK_TEXT_SIZE_NORMAL,
				SDL_TK_COLOUR_INTERFACE);

		entry->detail[SDL_TK_WIDGET_SELECTED] = sdl_tk_widget_detail(
				entry->widget,
				SDL_TK_TEXT_SIZE_NORMAL,
				SDL_TK_COLOUR_BACKGROUND);

		entry->detail[SDL_TK_WIDGET_DISABLED] = sdl_tk_widget_detail(
				entry->widget,
				SDL_TK_TEXT_SIZE_NORMAL,
				SDL_TK_COLOUR_DISABLED);

		if (title_max_width < title->w) {
			title_max_width = title->w;
		}
		entry_height = title->h;

		detail = entry->detail[SDL_TK_WIDGET_NORMAL];
		if (detail != NULL) {
			if (detail_max_width < detail->w) {
				detail_max_width = detail->w;
			}
			if (entry_height < detail->h) {
				entry_height = detail->h;
			}
		}

		height += entry_height;
	}

	if (max_width < title_max_width + EDGE_WIDTH * 4 + detail_max_width) {
		max_width = title_max_width + EDGE_WIDTH * 4 + detail_max_width;
	}

	menu->base.w = EDGE_WIDTH * 2 + max_width;
	menu->base.h = EDGE_WIDTH * 2 + height
			+ BORDER_WIDTH + GUTTER_WIDTH * 2;
}

/**
 * Layout an sdl-tk menu widget.
 *
 * \param[in]  widget  The widget to layout.
 */
static void sdl_tk_widget_menu_layout(
		struct sdl_tk_widget *widget)
{
	struct sdl_tk_widget_menu *menu = (struct sdl_tk_widget_menu *) widget;

	sdl_tk_widget_menu__layout(menu);
}

/**
 * Get the detail value for an sdl-tk menu widget.
 *
 * \param[in]  widget  The widget to get the detail for.
 * \param[in]  size    The text size to get the detail rendered at.
 * \param[in]  col     The text colour to get the detail rendered in.
 * \return an sdl-tk text object for the detail, or NULL on error.
 */
const struct sdl_tk_text *sdl_tk_widget_menu_detail(
		struct sdl_tk_widget *widget,
		sdl_tk_text_size_t    size,
		enum sdl_tk_colour    col)
{
	SDL_TK_UNUSED(widget);

	return sdl_tk_text_get_common(col, size, SDL_TK_TEXT_ARROW_RIGHT);
}

/**
 * Handle a keypress.
 *
 * \param[in]  menu  Menu widget.
 * \param[in]  key   The keypress to handle.
 * \return true if the widget handled the input, false otherwise.
 */
static bool sdl_tk_widget_menu_input_keypress(
		struct sdl_tk_widget_menu *menu,
		SDL_Keycode                key)
{
	switch (key) {
	case SDLK_UP:
		menu__nav_up(menu);
		break;

	case SDLK_DOWN:
		menu__nav_down(menu);
		break;

	case SDLK_RIGHT: /* Fall though */
	case SDLK_SPACE: /* Fall though */
	case SDLK_RETURN:
		sdl_tk_widget_action(menu->entries[menu->current].widget);
		break;

	case SDLK_LEFT:
		if (menu->base.parent != NULL) {
			struct sdl_tk_widget *parent = menu->base.parent;
			assert(parent->focus == SDL_TK_WIDGET_FOCUS_CHILD);

			menu->base.focus = SDL_TK_WIDGET_FOCUS_NONE;
			parent->focus = SDL_TK_WIDGET_FOCUS_TARGET;
		}
		break;

	default:
		return false;
	}

	return true;
}

/**
 * Fire an input event at an sdl-tk menu widget.
 *
 * \param[in]  widget  The widget to fire input at.
 * \param[in]  event   The input event to be handled.
 * \return true if the widget handled the input, false otherwise.
 */
static bool sdl_tk_widget_menu_input(
		struct sdl_tk_widget *widget,
		SDL_Event            *event)
{
	struct sdl_tk_widget_menu *menu = (struct sdl_tk_widget_menu *) widget;

	switch (widget->focus) {
	case SDL_TK_WIDGET_FOCUS_NONE:
		break;

	case SDL_TK_WIDGET_FOCUS_CHILD:
		return sdl_tk_widget_input(
				menu->entries[menu->current].widget,
				event);

	case SDL_TK_WIDGET_FOCUS_TARGET:
		switch (event->type) {
		case SDL_KEYDOWN:
			return sdl_tk_widget_menu_input_keypress(menu,
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
 * Set whether an sdl-tk menu widget has input focus.
 *
 * \param[in]  widget  The widget to set focus for.
 * \param[in]  focus   Whether the widget should have focus or not.
 */
static void sdl_tk_widget_menu_focus(
		struct sdl_tk_widget *widget,
		bool                  focus)
{
	struct sdl_tk_widget_menu *menu = (struct sdl_tk_widget_menu *) widget;

	if (widget->focus == SDL_TK_WIDGET_FOCUS_CHILD) {
		sdl_tk_widget_focus(
				menu->entries[menu->current].widget,
				false);

	}

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

/** The sdk-tk menu-type widget v-table. */
static const struct sdl_tk_widget_vt sdl_tk_widget_menu_vt = {
	.destroy = sdl_tk_widget_menu_destroy,
	.action  = sdl_tk_widget_menu_action,
	.detail  = sdl_tk_widget_menu_detail,
	.layout  = sdl_tk_widget_menu_layout,
	.render  = sdl_tk_widget_menu_render,
	.input   = sdl_tk_widget_menu_input,
	.focus   = sdl_tk_widget_menu_focus,
};

/* Exported function, documented in include/sdl-tk/widget/menu.h */
struct sdl_tk_widget *sdl_tk_widget_menu_create(
		struct sdl_tk_widget        *parent,
		const char                  *title,
		unsigned                     entry_count,
		sdl_tk_widget_menu_entry_cb  cb,
		void                        *pw)
{
	struct sdl_tk_widget_menu *menu;

	menu = calloc(1, sizeof(*menu));
	if (menu == NULL) {
		return NULL;
	}
	menu->base.t = &sdl_tk_widget_menu_vt;
	menu->base.parent = parent;
	menu->base.title = strdup(title);
	if (menu->base.title == NULL) {
		goto error;
	}

	menu->title = sdl_tk_text_create(title,
			sdl_tk_colour_get(SDL_TK_COLOUR_BACKGROUND),
			SDL_TK_TEXT_SIZE_NORMAL);
	if (menu->title == NULL) {
		goto error;
	}

	menu->entries = calloc(entry_count, sizeof(*menu->entries));
	if (menu->entries == NULL) {
		goto error;
	}

	menu->count = entry_count;
	for (unsigned i = 0; i < entry_count; i++) {
		struct sdl_tk_widget_menu_entry *entry = &menu->entries[i];
		const char *entry_title;

		entry->widget = cb(&menu->base, i, pw);
		if (entry->widget == NULL) {
			goto error;
		}

		entry_title = sdl_tk_widget_get_title(entry->widget);
		if (entry_title == NULL) {
			goto error;
		}

		entry->title[SDL_TK_WIDGET_NORMAL] = sdl_tk_text_create(
				entry_title,
				sdl_tk_colour_get(SDL_TK_COLOUR_INTERFACE),
				SDL_TK_TEXT_SIZE_NORMAL);
		if (entry->title[SDL_TK_WIDGET_NORMAL] == NULL) {
			goto error;
		}

		entry->title[SDL_TK_WIDGET_SELECTED] = sdl_tk_text_create(
				entry_title,
				sdl_tk_colour_get(SDL_TK_COLOUR_BACKGROUND),
				SDL_TK_TEXT_SIZE_NORMAL);
		if (entry->title[SDL_TK_WIDGET_SELECTED] == NULL) {
			goto error;
		}

		entry->title[SDL_TK_WIDGET_DISABLED] = sdl_tk_text_create(
				entry_title,
				sdl_tk_colour_get(SDL_TK_COLOUR_DISABLED),
				SDL_TK_TEXT_SIZE_NORMAL);
		if (entry->title[SDL_TK_WIDGET_DISABLED] == NULL) {
			goto error;
		}
	}

	sdl_tk_widget_menu__layout(menu);

	return &menu->base;

error:
	sdl_tk_widget_destroy(&menu->base);
	return NULL;
}
