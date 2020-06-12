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

#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>

#include "delay.h"

volatile bool elapsed;

void sys_tick_handler(void)
{
	elapsed = true;
}

/* Exported function, documented in systick.h */
void bl_delay_init(void)
{
	systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
}

/**
 * Delay for the given number of ticks.
 *
 * \param[in]  ticks  Ticks to delay for.
 */
static void bl__delay_ticks(uint32_t ticks)
{
	elapsed = false;

	systick_set_reload(ticks);
	systick_interrupt_enable();
	systick_counter_enable();

	/* TODO: Sleep the core until delay is done? */
	while (!elapsed);

	systick_interrupt_disable();
	systick_counter_disable();
}

/* Exported function, documented in systick.h */
void bl_delay_us(uint32_t us)
{
	/* The max delay in one interrupt is: 0xffffff / 72 = 233016 us */
	while (us > (STK_RVR_RELOAD / 72)) {
		bl__delay_ticks(STK_RVR_RELOAD / 72 * 72);
		us -= STK_RVR_RELOAD / 72;
	}

	bl__delay_ticks(us * 72);
}
