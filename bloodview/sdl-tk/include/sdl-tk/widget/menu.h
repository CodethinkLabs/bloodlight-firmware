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

#ifndef SDL_TK_WIDGET_MENU_H
#define SDL_TK_WIDGET_MENU_H

#include "sdl-tk/widget.h"

/**
 * Callback for acquiring widgets for the menu entries during menu creation.
 *
 * \param[in]  parent  The widget currently being created.
 * \param[in]  entry   The entry index to create a widget for.
 * \param[in]  pw      Client private context data.
 * \return the created sdl-tk widget for the menu entry or NULL on error.
 */
typedef struct sdl_tk_widget *(* sdl_tk_widget_menu_entry_cb)(
		struct sdl_tk_widget *parent,
		unsigned entry,
		void *pw);

/**
 * Create a menu-type widget.
 *
 * The given \ref cb is only called during the execution of this function,
 * while the menu widget is being constructed.
 *
 * \param[in]  parent       Parent widget, or NULL.
 * \param[in]  title        The widget's title.
 * \param[in]  entry_count  Number of entries in the menu.
 * \param[in]  cb           Callback for acquiring widgets for the menu entries.
 * \param[in]  pw           Client private context data passed back in cb.
 * \return a menu-type widget or NULL on error.
 */
struct sdl_tk_widget *sdl_tk_widget_menu_create(
		struct sdl_tk_widget        *parent,
		const char                  *title,
		unsigned                     entry_count,
		sdl_tk_widget_menu_entry_cb  cb,
		void                        *pw);

#endif /* SDL_TK_WIDGET_MENU_H */
