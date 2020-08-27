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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <pthread.h>

#include "graph.h"

/** Extra sample slots per graph */
#define GRAPH_EXCESS 1024

#define Y_SCALE_DATUM (1u << 10)
#define Y_SCALE_STEP  (1u <<  4)

struct graph {
	int32_t *data;

	unsigned max; /**< Maximum number of samples \ref data can store. */
	unsigned len; /**< Number of available samples (<= \ref max). */
	unsigned pos; /**< Position of next sample to insert. */
	unsigned ren; /**< Last rendered sample. */

	uint64_t scale;
	bool invert;
};

/** Graph global context. */
static struct {
	struct graph *channel;

	unsigned count;
	unsigned current;
	bool     single;

	pthread_mutex_t lock;
} graph_g;

/* Exported function, documented in graph.h */
void graph_fini(void)
{
	pthread_mutex_lock(&graph_g.lock);

	if (graph_g.channel != NULL) {
		for (unsigned i = 0; i < graph_g.count; i++) {
			free(graph_g.channel[i].data);
			memset(&graph_g.channel[i], 0,
					sizeof(graph_g.channel[i]));
		}
		free(graph_g.channel);
	}

	graph_g.channel = NULL;
	graph_g.count   = 0;

	pthread_mutex_unlock(&graph_g.lock);
}

/* Exported function, documented in graph.h */
bool graph_init(void)
{
	return true;
}

/* Exported function, documented in graph.h */
bool graph_create(unsigned idx, unsigned freq)
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
		unsigned max = GRAPH_EXCESS + freq * 32;
		g->data = malloc(max * sizeof(*g->data));
		if (g->data == NULL) {
			return false;
		}
		g->max = max;

		g->scale = Y_SCALE_DATUM / 8;
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
	unsigned y_next;
	unsigned y_prev;
	unsigned pos_next;
	const unsigned step = 1;
	struct graph *g = graph_g.channel + idx;

	if (idx >= graph_g.count) {
		return;
	}

	SDL_SetRenderDrawColor(ren, 255, 255, 255, SDL_ALPHA_OPAQUE);

	y_off += r->y;
	pos_next = graph_pos_decrement(g, g->pos);
	y_next = y_off + graph__data(g, pos_next) * g->scale / Y_SCALE_DATUM;

	for (unsigned x = r->x + r->w; x > (unsigned)r->x; x--) {
		for (unsigned i = 0; i < step - 1; i++) {
			pos_next = graph_pos_decrement(g, pos_next);
			y_prev = y_next;
			y_next = y_off + graph__data(g, pos_next) * g->scale / Y_SCALE_DATUM;
			SDL_RenderDrawLine(ren, x, y_prev, x, y_next);
		}

		pos_next = graph_pos_decrement(g, pos_next);
		y_prev = y_next;
		y_next = y_off + graph__data(g, pos_next) * g->scale / Y_SCALE_DATUM;
		SDL_RenderDrawLine(ren, x, y_prev, x - 1, y_next);
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

	if (g->scale < Y_SCALE_STEP) {
		g->scale = 1;
	} else {
		g->scale -= Y_SCALE_STEP;
	}

	return (g->scale != old);
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

/* Exported function, documented in graph.h */
bool graph_handle_input(
		const SDL_Event *event)
{
	bool handled = false;

	pthread_mutex_lock(&graph_g.lock);

	if ((graph_g.channel == NULL) || (graph_g.count == 0)) {
		handled = false;
		goto cleanup;
	}

	switch (event->type) {
	case SDL_KEYDOWN:
		switch (event->key.keysym.sym) {
		case SDLK_UP:
			handled = graph__y_scale_inc(graph_g.current);
			break;

		case SDLK_DOWN:
			handled = graph__y_scale_dec(graph_g.current);
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
			handled = graph__invert(graph_g.current);
			break;
		}
		break;
	}

cleanup:
	pthread_mutex_unlock(&graph_g.lock);
	return handled;
}
