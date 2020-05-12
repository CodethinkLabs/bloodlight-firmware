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

#include <libopencm3/stm32/rcc.h>

#include "acq.h"
#include "led.h"
#include "bl.h"
#include "usb.h"

/* Exported function, documented in bl.h */
void bl_init(void)
{
	rcc_clock_setup_pll(&rcc_hse8mhz_configs[RCC_CLOCK_HSE8_72MHZ]);

	bl_usb_init();
	bl_led_init();
	bl_acq_init();
}

int main(void)
{
	bl_init();

	while (true) {
		bl_usb_poll();
	}
}
