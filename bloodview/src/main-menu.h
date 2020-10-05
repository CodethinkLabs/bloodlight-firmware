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
 * \brief Interface to main menu module.
 *
 * This module implements the main menu.
 */

#ifndef MAIN_MENU_H
#define MAIN_MENU_H

#include "sdl-tk/colour.h"

#include "../../src/acq.h"

/**
 * Create the bloodlight main menu widget set.
 *
 * \param[in]  resources_dir_path  Path to resources directory.
 * \param[in]  config_dir_path     Path to config directory.
 * \param[in]  config_file         Config filename in config_dir_path or NULL.
 * \return the main menu root widget or NULL on error.
 */
struct sdl_tk_widget *main_menu_create(
		const char *resources_dir_path,
		const char *config_dir_path,
		const char *config_file);

/**
 * Destroy the bloodlight main menu widget set.
 *
 * \param[in]  main_menu  The main menu root widget.
 */
void main_menu_destroy(struct sdl_tk_widget *main_menu);

/**
 * Update the main menu state.
 */
void main_menu_update(void);

/**
 * Get the Acquisition Mode.
 *
 * \return the configured acquisition mode.
 */
enum bl_acq_mode main_menu_config_get_acq_mode(void);

/**
 * Get the LED mask.
 *
 * \return the led mask to use.
 */
uint16_t main_menu_config_get_led_mask(void);

/**
 * Get the source mask.
 *
 * \return the configured source mask.
 */
uint16_t main_menu_config_get_source_mask(void);

/**
 * Get the frequency.
 *
 * \return the configured frequency.
 */
uint16_t main_menu_config_get_frequency(void);

/**
 * Get the software oversample.
 *
 * \param[in]  source  The source to read config from.
 * \return the configured software oversample.
 */
uint32_t main_menu_config_get_source_sw_oversample(enum bl_acq_source source);

/**
 * Get the opamp gain.
 *
 * \param[in]  source  The source to read config from.
 * \return the configured opamp gain.
 */
uint32_t main_menu_config_get_source_opamp_gain(enum bl_acq_source source);

/**
 * Get the opamp offset.
 *
 * \param[in]  source  The source to read config from.
 * \return the configured opamp offset.
 */
uint32_t main_menu_config_get_source_opamp_offset(enum bl_acq_source source);

/**
 * Get the hardware oversample.
 *
 * \param[in]  source  The source to read config from.
 * \return the configured hardware oversample.
 */
uint32_t main_menu_config_get_source_hw_oversample(enum bl_acq_source source);

/**
 * Get the hardware shift.
 *
 * \param[in]  source  The source to read config from.
 * \return the configured hardware shift.
 */
uint32_t main_menu_config_get_source_hw_shift(enum bl_acq_source source);

/**
 * Get the shift for a given channel.
 *
 * \param[in]  channel  The channel to read config from.
 * \return the configured channel shift.
 */
uint8_t main_menu_config_get_channel_shift(uint8_t channel);

/**
 * Get the offset for a given channel.
 *
 * \param[in]  channel  The channel to read config from.
 * \return the configured channel offset.
 */
uint32_t main_menu_config_get_channel_offset(uint8_t channel);

/**
 * Get the sample value size for a given channel.
 *
 * \param[in]  channel  The channel to read config from.
 * \return the configured channel sample value size.
 */
bool main_menu_config_get_channel_sample32(uint8_t channel);

/**
 * Get whether the channel is inverted.
 *
 * \param[in]  channel  The channel to read config from.
 * \return the configured channel sample value size.
 */
bool main_menu_config_get_channel_inverted(uint8_t channel);

/**
 * Get channel render colour.
 *
 * \param[in]  channel  The channel to read config from.
 * \return the configured channel colour.
 */
SDL_Color main_menu_config_get_channel_colour(uint8_t channel);

/**
 * Get whether the normalisation filter is enabled.
 *
 * \return true if enabled, false otherwise.
 */
bool main_menu_config_get_filter_normalise_enabled(void);

/**
 * Get whether the AC noise suppression filter is enabled.
 *
 * \return true if enabled, false otherwise.
 */
bool main_menu_config_get_filter_ac_denoise_enabled(void);

/**
 * Get the frequency for sample normalisation.
 *
 * \return frequency in Hz.
 */
double main_menu_config_get_filter_normalise_frequency(void);

/**
 * Get the frequency for AC noise suppression.
 *
 * \return frequency in Hz.
 */
double main_menu_config_get_filter_ac_denoise_frequency(void);

/**
 * Set the shift configuration value for a given channel.
 *
 * \param[in]  channel  The channel update config for.
 * \param[in]  shift    The config value to set.
 * \return true if the config value was successfully updated, false otherwise.
 */
bool main_menu_config_set_channel_shift(uint8_t channel, uint8_t shift);

/**
 * Set the offset configuration value for a given channel.
 *
 * \param[in]  channel  The channel update config for.
 * \param[in]  offset   The config value to set.
 * \return true if the config value was successfully updated, false otherwise.
 */
bool main_menu_config_set_channel_offset(uint8_t channel, uint32_t offset);

/**
 * Set whether acquisition is currently possible.
 *
 * \param[in]  available  Whether it is possible to start an acqusition.
 */
void main_menu_set_acq_available(bool available);

#endif /* MAIN_MENU_H */
