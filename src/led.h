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

union bl_msg_data;
enum bl_error;

/** LED identifiers. */
enum bl_led_id {
	BL_LED_ID_0,
	BL_LED_ID_1,
	BL_LED_ID_2,
	BL_LED_ID_3,
	BL_LED_ID_4,
	BL_LED_ID_5,
	BL_LED_ID_6,
	BL_LED_ID_7,
	BL_LED_ID_8,
	BL_LED_ID_9,
	BL_LED_ID_10,
	BL_LED_ID_11,
	BL_LED_ID_12,
	BL_LED_ID_13,
	BL_LED_ID_14,
	BL_LED_ID_15,
	BL_LED__COUNT,
};

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
