
#include <assert.h>
#include <stdlib.h>

#include "sdl-tk/widget/action.h"
#include "sdl-tk/widget/custom.h"

#include "../text.h"
#include "../util.h"

/** The sdl-tk action-type widget object. */
struct sdl_tk_widget_action {
	struct sdl_tk_widget base;

	sdl_tk_widget_action_cb cb;
	void *pw;
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
 * \param[in]  x       X coordinate.
 * \param[in]  y       Y coordinate.
 */
static void sdl_tk_widget_action_render(
		struct sdl_tk_widget *widget,
		SDL_Renderer      *ren,
		unsigned           x,
		unsigned           y)
{
	SDL_TK_UNUSED(widget);
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
 * \return true if the widget handled the input, false otherwise.
 */
static bool sdl_tk_widget_action_input(
		struct sdl_tk_widget *widget,
		SDL_Event         *event)
{
	SDL_TK_UNUSED(widget);
	SDL_TK_UNUSED(event);

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
