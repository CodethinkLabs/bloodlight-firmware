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
 * \brief Header for the Menu widget.
 *
 * This is a menu widget that supports submenus.
 */

#ifndef SDL_TK_WIDGET_MENU_H
#define SDL_TK_WIDGET_MENU_H

#include "sdl-tk/widget.h"

/** Position at end of menu. */
#define SDL_TK_WIDGET_POS_END (UINT_MAX)

/**
 * Create a menu-type widget.
 *
 * \param[in]  parent  Parent widget, or NULL.
 * \param[in]  title   The widget's title.
 * \return a menu-type widget or NULL on error.
 */
struct sdl_tk_widget *sdl_tk_widget_menu_create(
		struct sdl_tk_widget *parent,
		const char           *title);

/**
 * Add a widget to a menu.
 *
 * \param[in]  widget     The menu widget to add an entry to.
 * \param[in]  new_entry  The new widget to add to the menu.
 * \param[in]  position   Entry position to add new menu entry at.
 *                        Note, to place at the end, use
 *                        \ref SDL_TK_WIDGET_POS_END.
 * \return true on success, or false on error.
 */
bool sdl_tk_widget_menu_add_entry(
		struct sdl_tk_widget *widget,
		struct sdl_tk_widget *new_entry,
		unsigned position);

#endif /* SDL_TK_WIDGET_MENU_H */
