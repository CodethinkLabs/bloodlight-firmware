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
 * \brief Header for the Toggle widget.
 *
 * This is basically a checkbox type on/off widget.
 */

#ifndef SDL_TK_WIDGET_TOGGLE_H
#define SDL_TK_WIDGET_TOGGLE_H

#include "sdl-tk/widget.h"

/**
 * Callback for toggle widget value change notification.
 *
 * \param[in]  pw         Client private context data.
 * \param[in]  new_value  New toggle widget value.
 */
typedef void (* sdl_tk_widget_toggle_cb)(
		void *pw,
		bool  new_value);

/**
 * Create a toggle-type widget.
 *
 * \param[in]  parent   Parent widget, or NULL.
 * \param[in]  title    The widget's title.
 * \param[in]  initial  The intial value of the toggle.
 * \param[in]  cb       Callback for value change notification.
 * \param[in]  pw       Client private context data passed back in cb.
 * \return a toggle-type widget or NULL on error.
 */
struct sdl_tk_widget *sdl_tk_widget_toggle_create(
		struct sdl_tk_widget          *parent,
		const char                    *title,
		bool                           initial,
		const sdl_tk_widget_toggle_cb  cb,
		void                          *pw);

/**
 * Update a toggle widget's value.
 *
 * \param[in]  widget  The toggle widget to update the value of.
 * \param[in]  value   The new value to set.
 * \return true if the widget's value was updated, false otherwise.
 */
bool sdl_tk_widget_toggle_set_value(
		struct sdl_tk_widget *widget,
		bool                  value);

#endif /* SDL_TK_WIDGET_TOGGLE_H */
