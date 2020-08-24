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

#ifndef SDL_TK_WIDGET_INPUT_H
#define SDL_TK_WIDGET_INPUT_H

#include "sdl-tk/widget.h"

/**
 * Callback for input widget change notification and validation.
 *
 * \param[in]  pw         Client private context data.
 * \param[in]  new_value  New input widget value.
 * \return true to accept the new value. false to reject it.
 */
typedef bool (* sdl_tk_widget_input_cb)(
		void       *pw,
		const char *new_value);

/**
 * Create an input-type widget.
 *
 * \param[in]  parent   Parent widget, or NULL.
 * \param[in]  title    The widget's title.
 * \param[in]  initial  The initial value of the input.
 * \param[in]  cb       Callback for input change notification and validation.
 * \param[in]  pw       Client private context data passed back in cb.
 * \return a input-type widget or NULL on error.
 */
struct sdl_tk_widget *sdl_tk_widget_input_create(
		struct sdl_tk_widget         *parent,
		const char                   *title,
		const char                   *initial,
		const sdl_tk_widget_input_cb  cb,
		void                         *pw);

/**
 * Update an input widget's value.
 *
 * \param[in]  widget  The input widget to update the value of.
 * \param[in]  value   The new value to set.
 * \return true if the widget's value was updated, false otherwise.
 */
bool sdl_tk_widget_input_set_value(
			struct sdl_tk_widget *widget,
			const char *value);

#endif /* SDL_TK_WIDGET_INPUT_H */
