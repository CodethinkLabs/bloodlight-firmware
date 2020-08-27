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

#ifndef BV_GRAPH_H
#define BV_GRAPH_H

#include <SDL2/SDL.h>

/**
 * Initialise the graph module.
 *
 * \return true on success or false on failure.
 */
bool graph_init(void);

/**
 * Finalise the graph module.
 */
void graph_fini(void);

/**
 * Create a graph at given index.
 *
 * \param[in]  idx   Graph index to create.
 * \param[in]  freq  The sampling frequency used for the graph.
 * \return true on success, or false on errer.
 */
bool graph_create(unsigned idx, unsigned freq);

/**
 * Add a sample to a graph.
 *
 * \param[in]  g_idx  Index of the graph to add sample to.
 * \param[in]  value  Sample to add to graph.
 * \return true on success, false on error.
 */
bool graph_data_add(unsigned g_idx, int32_t value);

/**
 * Render all the graphs
 *
 * \param[in]  ren  The SDL renderer.
 * \param[in]  r    The rectangle containing the graphs.
 */
void graph_render(
		SDL_Renderer   *ren,
		const SDL_Rect *r);

/**
 * Handle an input event
 *
 * \param[in]  event  The SDL renderer.
 * \param[in]  shift  True if the shift key is pressed.
 * \param[in]  ctrl   True if the control key is pressed.
 * \return true if the event was handled, or false otherwise.
 */
bool graph_handle_input(
		const SDL_Event *event,
		bool shift,
		bool ctrl);

#endif /* BV_GRAPH_H */
