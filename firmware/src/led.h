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

#ifndef BL_LED_H
#define BL_LED_H

#include <stdbool.h>
#include <stdint.h>

#include "common/error.h"
#include "common/led.h"

/** LED source globals */
typedef struct {
	uint8_t  led;
	uint8_t  src_mask;
	uint32_t gpios;
	uint32_t gpior;
	uint32_t gpio_bsrr;
} bl_led_channel_t;


extern unsigned bl_led_count;
extern volatile unsigned bl_led_active;
extern bl_led_channel_t bl_led_channel[];

/**
 * Initialise the LED module.
 */
void bl_led_init(void);

/**
 * Turn on/off LEDs.
 *
 * \param[in]  led_mask  Mask of LEDs to enable.
 * \return \ref BL_ERROR_NONE on success, or appropriate error otherwise.
 */
enum bl_error bl_led_set(uint16_t led_mask);

void bl_led_status_set(bool enable);

/**
 * Setup enabled LEDs list.
 *
 * \param[in]  led_mask  Mask of LEDs to enable.
 * \return \ref BL_ERROR_NONE on success, or appropriate error otherwise.
 */
enum bl_error bl_led_setup(uint16_t led_mask);

/**
 * Flash enabled LEDs in a fixed sequence.
 *
 * \return \ref BL_ERROR_NONE on success, or appropriate error otherwise.
 */
enum bl_error bl_led_loop(void);

/**
 * get the corresponding GPIO port for specified LED
 *
 * \param[in]   led LED index to be checked
 * \return \ref GPIO port for specified LED
 */
uint32_t bl_led_get_port(uint8_t led);

/**
 * Get the corresponding GPIO pin number for specified LED
 *
 * \param[in]   led LED index to be checked
 * \return \ref GPIO pin for specified LED
 */
uint16_t bl_led_get_gpio(uint8_t led);


#endif
