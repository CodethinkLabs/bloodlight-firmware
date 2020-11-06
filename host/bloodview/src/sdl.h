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
 * \brief Interface to the SDL module.
 *
 * This module handles interaction with SDL.
 */

#ifndef BV_SDL_H
#define BV_SDL_H

#include <stdbool.h>

/**
 * Finalise the SDL module.
 */
void sdl_fini(void);

/**
 * Initialise the SDL module.
 *
 * \param[in]  resources_dir_path  Path to resources directory.
 * \param[in]  config_dir_path     Path to config directory.
 * \param[in]  config_file         Config filename in config_dir_path or NULL.
 * \param[in]  font_path           Font path to use for the interface, or NULL.
 * \return true on success or false on failure.
 */
bool sdl_init(const char *resources_dir_path,
		const char *config_dir_path,
		const char *config_file,
		const char *font_path);

/**
 * Handle any SDL events.
 *
 * \return false if the program should quit, otherwise true.
 */
bool sdl_handle_input(void);

/**
 * Close the main menu.
 */
void sdl_main_menu_close(void);

/**
 * Render the display.
 */
void sdl_present(void);

#endif /* BV_SDL_H */
