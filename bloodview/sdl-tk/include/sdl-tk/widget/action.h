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

#ifndef SDL_TK_WIDGET_ACTION_H
#define SDL_TK_WIDGET_ACTION_H

#include "sdl-tk/widget.h"

/**
 * Callback for activation of the action-type widget.
 *
 * \param[in]  pw  Client private context data.
 */
typedef void (* sdl_tk_widget_action_cb)(
		void *pw);

/**
 * Create an action-type widget.
 *
 * \param[in]  parent   Parent widget, or NULL.
 * \param[in]  title    The widget's title.
 * \param[in]  cb       Callback for activation of the widget.
 * \param[in]  pw       Client private context data passed back in cb.
 * \return a action-type widget or NULL on error.
 */
struct sdl_tk_widget *sdl_tk_widget_action_create(
		struct sdl_tk_widget          *parent,
		const char                    *title,
		const sdl_tk_widget_action_cb  cb,
		void                          *pw);

#endif /* SDL_TK_WIDGET_ACTION_H */
