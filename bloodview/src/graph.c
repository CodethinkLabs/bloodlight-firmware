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
 * \brief Implementation of the graph module.
 *
 * This renders sample data in real time.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <pthread.h>

#include "sdl-tk/text.h"

#include "util.h"
#include "graph.h"
#include "main-menu.h"

/** Extra sample slots per graph */
#define GRAPH_EXCESS 1024

/** Reference for scaling in the vertical dimension. */
#define Y_SCALE_DATUM (1u << 10)

/** Step size for vertical scale increments. */
#define Y_SCALE_STEP  (1u <<  4)

/** Maximum number of seconds of graph data to store for each channel. */
#define GRAPH_HISTORY_SECONDS 64

/** Per-graph data context. */
struct graph {
	int32_t *data; /**< The graph's sample data. */

	unsigned max; /**< Maximum number of samples \ref data can store. */
	unsigned len; /**< Number of available samples (<= \ref max). */
	unsigned pos; /**< Position of next sample to insert. */
	unsigned ren; /**< Last rendered sample. */

	unsigned x_step; /**< Rendering scale in time dimension. */
	uint64_t scale;  /**< The vertical scale. */
	bool invert;     /**< Whether to invert the magnitudes. */

	uint8_t channel_idx; /**< The acquisition channel index. */
	SDL_Color colour;    /**< Graph render colour. */
};

/** Per-graph render context. */
struct render {
	struct sdl_tk_text *label; /**< The graph name. */
};

/** Graph global context. */
static struct {
	struct graph *channel;

	unsigned count;
	unsigned current;
	bool     single;

	struct render *render;
	unsigned render_count;
	bool render_finalise;

	pthread_mutex_t lock;
} graph_g;

/* Exported function, documented in graph.h */
void graph_fini(void)
{
	pthread_mutex_lock(&graph_g.lock);

	if (graph_g.channel != NULL) {
		for (unsigned i = 0; i < graph_g.count; i++) {
			struct graph *g = graph_g.channel + i;
			int32_t *data = g->data;

			g->data = NULL;

			free(data);

			memset(g, 0, sizeof(*g));
		}
		free(graph_g.channel);
	}

	graph_g.channel = NULL;
	graph_g.count   = 0;
	graph_g.single  = false;

	graph_g.render_finalise = true;

	pthread_mutex_unlock(&graph_g.lock);
	pthread_mutex_destroy(&graph_g.lock);
}

/* Exported function, documented in graph.h */
bool graph_init(void)
{
	if (pthread_mutex_init(&graph_g.lock, NULL) != 0) {
		return false;
	}

	return true;
}

/* Exported function, documented in graph.h */
bool graph_create(unsigned idx, unsigned freq, uint8_t channel)
{
	struct graph *g;

	if (idx >= graph_g.count) {
		struct graph *channels;
		unsigned count = idx + 1;
		size_t size = sizeof(*channels);
		unsigned old_count = graph_g.count;

		channels = realloc(graph_g.channel, count * size);
		if (channels == NULL) {
			return false;
		}

		memset(&channels[old_count], 0, (count - old_count) * size);

		graph_g.channel = channels;
		graph_g.count   = count;
	}

	g = graph_g.channel + idx;

	if (g->data == NULL) {
		unsigned max = GRAPH_EXCESS + freq * GRAPH_HISTORY_SECONDS;

		g->max = max;
		g->x_step = freq / 500 + 1;
		g->scale = Y_SCALE_DATUM / 8;
		g->channel_idx = channel;
		g->colour = main_menu_config_get_channel_colour(channel);

		g->data = calloc(max, sizeof(*g->data));
		if (g->data == NULL) {
			return false;
		}
	} else {
		return false;
	}

	return true;
}

/**
 * Ensure that a graph at the given index exists.
 *
 * \param[in]  idx  Index to check.
 * \return true if the graph exists, otherwise false.
 */
static bool graph__ensure(unsigned idx)
{
	if ((idx >= graph_g.count) || graph_g.channel[idx].data == NULL) {
		return false;
	}

	return true;
}

/**
 * Increment the position pointer of a graph object.
 *
 * \param[in]  graph  The graph object.
 * \param[in]  pos    The position to increment.
 * \return the new position.
 */
static inline unsigned graph_pos_increment(
		const struct graph *graph,
		unsigned pos)
{
	pos++;

	if (pos == graph->max) {
		pos = 0;
	}

	return pos;
}

/**
 * Decrement the position pointer of a graph object.
 *
 * \param[in]  graph  The graph object.
 * \param[in]  pos    The position to decrement.
 * \return the new position.
 */
static inline unsigned graph_pos_decrement(
		const struct graph *graph,
		unsigned pos)
{
	if (pos == 0) {
		pos = graph->max - 1;
	} else {
		pos--;
	}

	return pos;
}

/* Exported function, documented in graph.h */
bool graph_data_add(unsigned idx, int32_t value)
{
	struct graph *g = graph_g.channel + idx;

	if (!graph__ensure(idx)) {
		return false;
	}

	g->data[g->pos] = value;

	if (g->len < g->max) {
		g->len++;
	}

	g->pos = graph_pos_increment(g, g->pos);

	return true;
}

/**
 * Get a graph data value.
 *
 * \param[in]  g    The graph to get a data value from.
 * \param[in]  pos  The position of the data value to get.
 * \return a data value.
 */
static inline int32_t graph__data(const struct graph *g, unsigned pos)
{
	if (g->invert) {
		return g->data[pos] * -1;
	}

	return g->data[pos];
}

/**
 * Helper for creating channel label texture.
 *
 * \param[in]  channel  The channel index.
 * \param[in]  colour   The channel colour.
 * \return new text label, or NULL on error.
 */
static struct sdl_tk_text *graph__create_label(
		uint8_t   channel,
		SDL_Color colour)
{
	struct sdl_tk_text *text;
	const char *string;

	string = main_menu_config_get_channel_name(channel);
	if (string == NULL) {
		return NULL;
	}

	text = sdl_tk_text_create(string, colour, SDL_TK_TEXT_SIZE_NORMAL);
	if (text == NULL) {
		return NULL;
	}

	return text;
}

/**
 * Render a graph's label, creating it if needed.
 *
 * \param[in]  ren  The SDL renderer.
 * \param[in]  idx  The graph index to render the label for.
 * \param[in]  g    The graph to render the label for.
 * \param[in]  r    The rectangle containing the graphs.
 * \return new text label, or NULL on error.
 */
static void graph__render_label(
		SDL_Renderer   *ren,
		unsigned        idx,
		struct graph   *g,
		const SDL_Rect *r)
{
	struct render *render;

	if (graph_g.render_count <= idx) {
		unsigned count = idx + 1;
		size_t size = sizeof(*render);
		unsigned old_count = graph_g.render_count;

		render = realloc(graph_g.render, count * size);
		if (render == NULL) {
			return;
		}

		memset(&render[old_count], 0, (count - old_count) * size);

		graph_g.render       = render;
		graph_g.render_count = count;
	}

	render = graph_g.render + idx;

	if (render->label == NULL) {
		render->label = graph__create_label(g->channel_idx, g->colour);
		if (render->label == NULL) {
			return;
		}
	}

	SDL_Rect rect = {
		.x = r->x + 2,
		.y = r->y + 2,
		.w = render->label->w,
		.h = render->label->h,
	};

	SDL_RenderCopy(ren, render->label->t, NULL, &rect);
}

/**
 * Finalise the render components of graphs.
 *
 * This is work that must be done in the rendering thread.
 */
static void graph__render_fini(void)
{
	if (graph_g.render_finalise) {
		if (graph_g.render != NULL) {
			for (unsigned i = 0; i < graph_g.render_count; i++) {
				struct render *r = graph_g.render + i;
				struct sdl_tk_text *label = r->label;

				r->label = NULL;

				sdl_tk_text_destroy(label);
			}
			free(graph_g.render);
			graph_g.render = NULL;
		}

		graph_g.render_count = 0;
		graph_g.render_finalise = false;
	}
}

/**
 * Render a graph.
 *
 * \param[in]  ren    The SDL renderer.
 * \param[in]  idx    The graph index to render.
 * \param[in]  r      The rectangle containing the graphs.
 * \param[in]  y_off  The vertical offset at which to render the graph.
 */
void graph__render(
		SDL_Renderer   *ren,
		unsigned        idx,
		const SDL_Rect *r,
		unsigned        y_off)
{
	unsigned len;
	unsigned x_step;
	unsigned x_min;
	unsigned y_next;
	unsigned y_prev;
	unsigned pos_next;
	struct graph *g = graph_g.channel + idx;

	if (idx >= graph_g.count) {
		return;
	}
	if (g->data == NULL) {
		return;
	}

	graph__render_label(ren, idx, g, r);

	x_step = g->x_step;

	SDL_SetRenderDrawColor(ren,
			g->colour.r,
			g->colour.g,
			g->colour.b,
			SDL_ALPHA_OPAQUE);

	len = 0;
	pos_next = graph_pos_decrement(g, g->pos);

	y_off += r->y;
	y_next = y_off + graph__data(g, pos_next) * g->scale / Y_SCALE_DATUM;

	x_min = r->x;
	for (unsigned x = r->x + r->w; x > x_min && len < g->len; x--) {
		for (unsigned i = 0; i < x_step - 1; i++) {
			pos_next = graph_pos_decrement(g, pos_next);
			y_prev = y_next;
			y_next = y_off + graph__data(g, pos_next) * g->scale / Y_SCALE_DATUM;
			SDL_RenderDrawLine(ren, x, y_prev, x, y_next);
			len++;
		}

		pos_next = graph_pos_decrement(g, pos_next);
		y_prev = y_next;
		y_next = y_off + graph__data(g, pos_next) * g->scale / Y_SCALE_DATUM;
		SDL_RenderDrawLine(ren, x, y_prev, x - 1, y_next);
		len++;
	}
}

/* Exported function, documented in graph.h */
void graph_render(
		SDL_Renderer   *ren,
		const SDL_Rect *r)
{
	SDL_Rect graph_rect = *r;

	pthread_mutex_lock(&graph_g.lock);

	if ((graph_g.channel == NULL) ||
	    (graph_g.count == 0)) {
		goto cleanup;
	}

	if (graph_g.single) {
		graph__render(ren, graph_g.current, r, r->h / 2);
		goto cleanup;
	}

	graph_rect.h = r->h / graph_g.count;
	graph_rect.y = (r->h - graph_rect.h * graph_g.count) / 2 +
			graph_rect.h * graph_g.current;

	SDL_SetRenderDrawColor(ren, 32, 32, 32, SDL_ALPHA_OPAQUE);
	SDL_RenderFillRect(ren, &graph_rect);

	graph_rect.y = (r->h - graph_rect.h * graph_g.count) / 2;
	for (unsigned i = 0; i < graph_g.count; i++) {
		graph__render(ren, i, &graph_rect, graph_rect.h / 2);
		graph_rect.y += graph_rect.h;
	}

cleanup:
	graph__render_fini();
	pthread_mutex_unlock(&graph_g.lock);
}

/**
 * Increment a graphs's y-scale.
 *
 * \param[in]  idx  Index of graph to scale.
 * \return true if scale changed, false otherwise.
 */
static bool graph__y_scale_inc(unsigned idx)
{
	struct graph *g = graph_g.channel + idx;
	unsigned old = g->scale;

	if (idx >= graph_g.count) {
		return false;
	}

	g->scale += Y_SCALE_STEP;

	if (g->scale > Y_SCALE_DATUM * 8) {
		g->scale = Y_SCALE_DATUM * 8;
	}

	return (g->scale != old);
}

/**
 * Decrement a graphs's y-scale.
 *
 * \param[in]  idx  Index of graph to scale.
 * \return true if scale changed, false otherwise.
 */
static bool graph__y_scale_dec(unsigned idx)
{
	struct graph *g = graph_g.channel + idx;
	unsigned old = g->scale;

	if (idx >= graph_g.count) {
		return false;
	}

	if (g->scale <= Y_SCALE_STEP) {
		g->scale = 1;
	} else {
		g->scale -= Y_SCALE_STEP;
	}

	return (g->scale != old);
}

/**
 * Increment a graphs's x-scale.
 *
 * \param[in]  idx  Index of graph to scale.
 * \return true if scale changed, false otherwise.
 */
static bool graph__x_scale_inc(unsigned idx)
{
	struct graph *g = graph_g.channel + idx;
	unsigned old = g->x_step;

	if (idx >= graph_g.count) {
		return false;
	}

	g->x_step++;

	if (g->x_step > 128) {
		g->x_step = 128;
	}

	return (g->x_step != old);
}

/**
 * Decrement a graphs's x-scale.
 *
 * \param[in]  idx  Index of graph to scale.
 * \return true if scale changed, false otherwise.
 */
static bool graph__x_scale_dec(unsigned idx)
{
	struct graph *g = graph_g.channel + idx;
	unsigned old = g->x_step;

	if (idx >= graph_g.count) {
		return false;
	}

	g->x_step--;

	if (g->x_step == 0) {
		g->x_step = 1;
	}

	return (g->x_step != old);
}

/**
 * Toggle a graph's inversion state.
 *
 * \param[in]  idx  The graph index to invert.
 * \return true.
 */
static bool graph__invert(unsigned idx)
{
	struct graph *g = graph_g.channel + idx;

	g->invert = !g->invert;

	return true;
}

/**
 * Call a hander function for keyboard input, on appropriate graphs.
 *
 * \param[in]  shift  Whether shift key is pressed.
 * \param[in]  ctrl   Whether ctrl key is pressed.
 * \param[in]  func   Function to call for appropriate graphs.
 */
static bool graph__key_handler(
		bool shift,
		bool ctrl,
		bool (*func)(unsigned idx))
{
	bool ret = false;

	BV_UNUSED(ctrl);

	if (shift) {
		for (unsigned i = 0; i < graph_g.count; i++) {
			ret |= func(i);
		}
	} else {
		ret |= func(graph_g.current);
	}

	return ret;
}

/**
 * Handle a keyboard input event.
 *
 * \param[in]  event  The SDL renderer.
 * \param[in]  shift  True if the shift key is pressed.
 * \param[in]  ctrl   True if the control key is pressed.
 * \return true if the event was handled, or false otherwise.
 */
static bool graph__handle_key(
		const SDL_Event *event,
		bool shift,
		bool ctrl)
{
	bool handled = false;

	switch (event->key.keysym.sym) {
	case SDLK_UP:
		handled = graph__key_handler(shift, ctrl, graph__y_scale_inc);
		break;

	case SDLK_DOWN:
		handled = graph__key_handler(shift, ctrl, graph__y_scale_dec);
		break;

	case SDLK_LEFT:
		handled = graph__key_handler(shift, ctrl, graph__x_scale_inc);
		break;

	case SDLK_RIGHT:
		handled = graph__key_handler(shift, ctrl, graph__x_scale_dec);
		break;

	case SDLK_PAGEUP:
		if (graph_g.current == 0) {
			graph_g.current = graph_g.count - 1;
		} else {
			graph_g.current--;
		}
		handled = true;
		break;

	case SDLK_PAGEDOWN:
		graph_g.current++;
		if (graph_g.current == graph_g.count) {
			graph_g.current = 0;
		}
		handled = true;
		break;

	case SDLK_SPACE: /* Fall though */
	case SDLK_RETURN:
		graph_g.single = !graph_g.single;
		handled = true;
		break;

	case SDLK_i:
		handled = graph__key_handler(shift, ctrl, graph__invert);
		break;
	}

	return handled;
}

/**
 * Handle a mouse input event.
 *
 * \param[in]  event  The SDL renderer.
 * \param[in]  r      The rectangle containing the graphs.
 * \param[in]  shift  True if the shift key is pressed.
 * \param[in]  ctrl   True if the control key is pressed.
 * \return true if the event was handled, or false otherwise.
 */
static bool graph__handle_mouse(
		const SDL_Event *event,
		const SDL_Rect *r,
		bool shift,
		bool ctrl)
{
	bool handled = false;
	unsigned idx;
	int x;
	int y;
	int h;

	SDL_GetMouseState(&x, &y);

	if (graph_g.count == 0) {
		goto cleanup;
	}

	if (x < r->x || x >= r->x + r->w || y < r->y || y >= r->y + r->h) {
		/* Outside graphs area. */
		goto cleanup;
	}

	h = r->h / graph_g.count;
	idx = (y - r->y) / h;

	if (idx >= graph_g.count) {
		idx = graph_g.count - 1;
	}

	if (graph_g.current != idx) {
		graph_g.current = idx;
		handled = true;
	}

	switch (event->type) {
	case SDL_MOUSEWHEEL:
		if (event->wheel.y > 0) {
			handled = graph__key_handler(shift, ctrl,
					graph__y_scale_inc);
		} else if(event->wheel.y < 0) {
			handled = graph__key_handler(shift, ctrl,
					graph__y_scale_dec);
		}

		if (event->wheel.x > 0) {
			handled = graph__key_handler(shift, ctrl,
					graph__x_scale_inc);
		} else if(event->wheel.x < 0) {
			handled = graph__key_handler(shift, ctrl,
					graph__x_scale_dec);
		}
		break;

	case SDL_MOUSEBUTTONDOWN:
		switch (event->button.button) {
		case SDL_BUTTON_LEFT:
			graph_g.single = !graph_g.single;
			handled = true;
			break;

		case SDL_BUTTON_MIDDLE:
			handled = graph__key_handler(shift, ctrl,
					graph__invert);
			break;
		}
		break;
	}

cleanup:
	return handled;
}

/* Exported function, documented in graph.h */
bool graph_handle_input(
		const SDL_Event *event,
		const SDL_Rect *r,
		bool shift,
		bool ctrl)
{
	bool handled = false;

	pthread_mutex_lock(&graph_g.lock);

	if ((graph_g.channel == NULL) || (graph_g.count == 0)) {
		handled = false;
		goto cleanup;
	}

	switch (event->type) {
	case SDL_KEYDOWN:
		handled = graph__handle_key(event, shift, ctrl);
		break;

	case SDL_MOUSEWHEEL:    /* Fall through. */
	case SDL_MOUSEMOTION:   /* Fall through. */
	case SDL_MOUSEBUTTONUP: /* Fall through. */
	case SDL_MOUSEBUTTONDOWN:
		handled = graph__handle_mouse(event, r, shift, ctrl);
		break;
	}

cleanup:
	pthread_mutex_unlock(&graph_g.lock);
	return handled;
}
