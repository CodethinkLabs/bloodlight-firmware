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

#ifndef MAIN_MENU_H
#define MAIN_MENU_H

/**
 * Create the bloodlight main menu widget set.
 *
 * \return the main menu root widget or NULL on error.
 */
struct sdl_tk_widget *main_menu_create(void);

/**
 * Get the LED mask.
 *
 * \return the led mask to use.
 */
uint16_t main_menu_conifg_get_led_mask(void);

/**
 * Get the source mask.
 *
 * \return the configured led mask.
 */
uint16_t main_menu_conifg_get_source_mask(void);

/**
 * Get the frequency.
 *
 * \return the configured frequency.
 */
uint16_t main_menu_conifg_get_frequency(void);

/**
 * Get the oversample.
 *
 * \return the configured oversample.
 */
uint32_t main_menu_conifg_get_oversample(void);

/**
 * Get the gain for a given channel.
 *
 * \param[in]  channel  The channel to read config from.
 * \return the configured channel gain.
 */
uint8_t main_menu_conifg_get_channel_gain(uint8_t channel);

/**
 * Get the shift for a given channel.
 *
 * \param[in]  channel  The channel to read config from.
 * \return the configured channel shift.
 */
uint8_t main_menu_conifg_get_channel_shift(uint8_t channel);

/**
 * Get the offset for a given channel.
 *
 * \param[in]  channel  The channel to read config from.
 * \return the configured channel offset.
 */
uint32_t main_menu_conifg_get_channel_offset(uint8_t channel);

/**
 * Get the sample value size for a given channel.
 *
 * \param[in]  channel  The channel to read config from.
 * \return the configured channel sample value size.
 */
bool main_menu_conifg_get_channel_sample32(uint8_t channel);

/**
 * Get whether the normalisation filter is enabled.
 *
 * \return true if enabled, false otherwise.
 */
bool main_menu_conifg_get_filter_normalise_enabled(void);

/**
 * Get whether the AC noise suppression filter is enabled.
 *
 * \return true if enabled, false otherwise.
 */
bool main_menu_conifg_get_filter_ac_denoise_enabled(void);

/**
 * Get the frequency for sample normalisation.
 *
 * \return frequency in Hz.
 */
double main_menu_conifg_get_filter_normalise_frequency(void);

/**
 * Get the frequency for AC noise suppression.
 *
 * \return frequency in Hz.
 */
double main_menu_conifg_get_filter_ac_denoise_frequency(void);

/**
 * Set the shift configuration value for a given channel.
 *
 * \param[in]  channel  The channel update config for.
 * \param[in]  shift    The config value to set.
 * \return true if the config value was successfully updated, false otherwise.
 */
bool main_menu_conifg_set_channel_shift(uint8_t channel, uint8_t shift);

/**
 * Set the offset configuration value for a given channel.
 *
 * \param[in]  channel  The channel update config for.
 * \param[in]  offset   The config value to set.
 * \return true if the config value was successfully updated, false otherwise.
 */
bool main_menu_conifg_set_channel_offset(uint8_t channel, uint32_t offset);

/**
 * Set whether acquisition is currently possible.
 *
 * \param[in]  available  Whether it is possible to start an acqusition.
 */
void main_menu_set_acq_available(bool available);

#endif /* MAIN_MENU_H */
