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
 * \brief Header for the Select widget.
 *
 * This is a widget that allows one of a set of options to be slected.
 */

#ifndef SDL_TK_WIDGET_SELECT_H
#define SDL_TK_WIDGET_SELECT_H

#include "sdl-tk/widget.h"

/**
 * Callback for select widget value change notification.
 *
 * \param[in]  pw         Client private context data.
 * \param[in]  new_value  New select widget value.
 */
typedef void (* sdl_tk_widget_select_cb)(
		void     *pw,
		unsigned  new_value);

/**
 * Create a select-type widget.
 *
 * \param[in]  parent         Parent widget, or NULL.
 * \param[in]  title          The widget's title.
 * \param[in]  options        Array of option titles.
 * \param[in]  options_count  Number of entries in options array.
 * \param[in]  initial        Initial widget value.
 * \param[in]  cb             Callback for selection changes.
 * \param[in]  pw             Client private context data passed back in cb.
 * \return a select-type widget or NULL on error.
 */
struct sdl_tk_widget *sdl_tk_widget_select_create(
		struct sdl_tk_widget          *parent,
		const char                    *title,
		const char * const            *options,
		unsigned                       options_count,
		unsigned                       initial,
		const sdl_tk_widget_select_cb  cb,
		void                          *pw);

/**
 * Update a select widget's value.
 *
 * \param[in]  widget  The select widget to update the value of.
 * \param[in]  value   The new value to set.
 * \return true if the widget's value was updated, false otherwise.
 */
bool sdl_tk_widget_select_set_value(
		struct sdl_tk_widget *widget,
		unsigned              value);

#endif /* SDL_TK_WIDGET_SELECT_H */
