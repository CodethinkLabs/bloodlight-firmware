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
#include "delay.h"
#include "error.h"

const struct rcc_clock_scale rcc_hse16mhz_config = {
	.pllsrc = RCC_CFGR_PLLSRC_HSE_PREDIV,
	.pllmul = RCC_CFGR_PLLMUL_MUL9,
	.plldiv = RCC_CFGR2_PREDIV_DIV2,
	.usbdiv1 = false,
	.flash_waitstates = 2,
	.hpre = RCC_CFGR_HPRE_DIV_NONE,
	.ppre1 = RCC_CFGR_PPRE1_DIV_2,
	.ppre2 = RCC_CFGR_PPRE2_DIV_NONE,
	.ahb_frequency = 72e6,
	.apb1_frequency = 36e6,
	.apb2_frequency = 72e6,
};

/* Exported function, documented in bl.h */
void bl_init(void)
{
	rcc_clock_setup_pll(&rcc_hse16mhz_config);

	bl_delay_init();
	bl_usb_init();
	bl_led_init();
	bl_acq_init();
}

int main(void)
{
	bl_init();

	while (true) {
		bl_acq_poll();
		bl_usb_poll();
	}
}
