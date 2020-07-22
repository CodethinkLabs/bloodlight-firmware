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

#include <stdint.h>

#include "error.h"

union bl_msg_data;

/** LED identifiers. */
enum bl_led_id {
	BL_LED_ID_0, /* LED 1, 850nm, IR */
	BL_LED_ID_1, /* LED 2, 880nm, IR */
	BL_LED_ID_2, /* LED 3, 940nm, IR */
	BL_LED_ID_3, /* LED 4, 1040nm, IR */
	BL_LED_ID_4, /* LED 5, 1200nm, IR */
	BL_LED_ID_5, /* LED 6, 1450nm, IR */
	BL_LED_ID_6, /* LED 7, 1550nm, IR */
	BL_LED_ID_7, /* LED 8, 1650nm, IR */
	BL_LED_ID_8, /* LED 9, 740nm, DULL_RED */
	BL_LED_ID_9, /* LED 10, 660nm,  DEEP_RED*/
	BL_LED_ID_10, /* LED 11, 638nm, LIGHT_RED */
	BL_LED_ID_11, /* LED 12, 612nm, DEEP_ORANGE */
	BL_LED_ID_12, /* LED 13, 590nm, LIGHT_ORANGE */
	BL_LED_ID_13, /* LED 14, 570nm, YELLOW */
	BL_LED_ID_14, /* LED 15, 528nm, GREEN */
	BL_LED_ID_15, /* LED 16, 470nm, BLUE */

	BL_LED__COUNT
};

/* Friendly names for visible light LEDs */
#define LED_VISIBLE_BLUE            BL_LED_ID_15
#define LED_VISIBLE_GREEN           BL_LED_ID_14
#define LED_VISIBLE_YELLOW          BL_LED_ID_13
#define LED_VISIBLE_LIGHT_ORANGE    BL_LED_ID_12
#define LED_VISIBLE_DEEP_ORANGE     BL_LED_ID_11
#define LED_VISIBLE_LIGHT_RED       BL_LED_ID_10
#define LED_VISIBLE_DEEP_RED        BL_LED_ID_9
#define LED_VISIBLE_DULL_RED        BL_LED_ID_8

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

#endif
