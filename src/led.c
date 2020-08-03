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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

#include "error.h"
#include "util.h"
#include "led.h"
#include "msg.h"

/** GPIO ports used for LEDs */
enum led_port {
	LED_PORT_A,
	LED_PORT_B,
	LED_PORT_C,
};

/** GPIO port addresses */
static const uint32_t led_port[] = {
	[LED_PORT_A] = GPIOA,
	[LED_PORT_B] = GPIOB,
	[LED_PORT_C] = GPIOC,
};

/** Table of LED ports/pins. */
static const struct led_table {
	uint8_t port_idx;
	uint8_t pin;
} led_table[BL_LED_COUNT] = {
#if (BL_REVISION == 1)
	[ 0] = { .port_idx = LED_PORT_B, .pin =  1 },
	[ 1] = { .port_idx = LED_PORT_B, .pin =  2 },
	[ 2] = { .port_idx = LED_PORT_B, .pin = 10 },
	[ 3] = { .port_idx = LED_PORT_B, .pin = 11 },
	[ 4] = { .port_idx = LED_PORT_A, .pin =  8 },
	[ 5] = { .port_idx = LED_PORT_A, .pin =  9 },
	[ 6] = { .port_idx = LED_PORT_A, .pin = 10 },
	[ 7] = { .port_idx = LED_PORT_A, .pin = 15 },
	[ 8] = { .port_idx = LED_PORT_C, .pin = 13 },
	[ 9] = { .port_idx = LED_PORT_C, .pin = 14 },
	[10] = { .port_idx = LED_PORT_C, .pin = 15 },
	[11] = { .port_idx = LED_PORT_B, .pin =  9 },
	[12] = { .port_idx = LED_PORT_B, .pin =  8 },
	[13] = { .port_idx = LED_PORT_B, .pin =  7 },
	[14] = { .port_idx = LED_PORT_B, .pin =  6 },
	[15] = { .port_idx = LED_PORT_B, .pin =  5 },
#else
	[ 0] = { .port_idx = LED_PORT_C, .pin = 14 },
	[ 1] = { .port_idx = LED_PORT_A, .pin = 10 },
	[ 2] = { .port_idx = LED_PORT_B, .pin = 11 },
	[ 3] = { .port_idx = LED_PORT_C, .pin = 13 },
	[ 4] = { .port_idx = LED_PORT_A, .pin =  6 },
	[ 5] = { .port_idx = LED_PORT_A, .pin =  8 },
	[ 6] = { .port_idx = LED_PORT_A, .pin =  9 },
	[ 7] = { .port_idx = LED_PORT_A, .pin =  5 },
	[ 8] = { .port_idx = LED_PORT_C, .pin = 15 },
	[ 9] = { .port_idx = LED_PORT_B, .pin =  6 },
	[10] = { .port_idx = LED_PORT_B, .pin =  5 },
	[11] = { .port_idx = LED_PORT_A, .pin =  0 },
	[12] = { .port_idx = LED_PORT_B, .pin =  4 },
	[13] = { .port_idx = LED_PORT_A, .pin = 15 },
	[14] = { .port_idx = LED_PORT_A, .pin =  1 },
	[15] = { .port_idx = LED_PORT_A, .pin =  2 },
#endif
};

static inline uint16_t bl_led__get_pin_mask(
		enum led_port port,
		uint16_t led_mask)
{
	uint16_t pin_mask = 0;

	for (unsigned i = 0; i < BL_LED_COUNT; i++) {
		if ((1U << i) & led_mask) {
			if (led_table[i].port_idx == port) {
				pin_mask |= (1 << led_table[i].pin);
			}
		}
	}

	return pin_mask;
}

static inline void bl_led__gpio_mode_setup(enum led_port port)
{
	gpio_mode_setup(led_port[port], GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,
			bl_led__get_pin_mask(port, 0xffff));
}

/* Exported function, documented in led.h */
void bl_led_init(void)
{
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_GPIOC);

	bl_led__gpio_mode_setup(LED_PORT_A);
	bl_led__gpio_mode_setup(LED_PORT_B);
	bl_led__gpio_mode_setup(LED_PORT_C);

	bl_led_set(0x0000);
}

static inline void bl_led__set(
		enum led_port port,
		uint16_t led_mask)
{
	/* We can trivially optimise this, if needed, by:
	 *
	 * 1. Making bl_led__get_pin_mask() build all three port masks at once.
	 * 2. Making bl_led_init() cache the clear masks so they can be reused
	 *    here.
	 */
	gpio_clear(led_port[port], bl_led__get_pin_mask(port, 0xffff));
	gpio_set(led_port[port], bl_led__get_pin_mask(port, led_mask));
}

/* Exported function, documented in led.h */
enum bl_error bl_led_set(uint16_t led_mask)
{
	bl_led__set(LED_PORT_A, led_mask);
	bl_led__set(LED_PORT_B, led_mask);
	bl_led__set(LED_PORT_C, led_mask);

	return BL_ERROR_NONE;
}

void bl_led_status_set(bool enable)
{
#if (BL_REVISION >= 2)
	if (enable) {
		gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO7);
		gpio_clear(GPIOB, GPIO7);
	} else {
		gpio_mode_setup(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO7);
	}
#else
	BL_UNUSED(enable);
#endif
}
