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

#include <libopencm3/cm3/systick.h>
#include <libopencm3/cm3/nvic.h>

#include "tick.h"

/* Nanosecond counter */
static uint32_t us;

/* Millisecond counter */
static uint32_t ms;
static uint32_t last_ms_us = 0;

#define MAX_TIMER_RES_US 1000
#define MIN_TIMER_RES_US   10
static uint32_t res = MAX_TIMER_RES_US;

static uint32_t sys_tick_frequency;


/**
 * systick isr
 */
void sys_tick_handler(void)
{
	us += res;
	/* We could just use us only and multiply everywhere, but doing it this
	 * way might be handy later if we wanna do ms specific stuff
	 */
	if (us - last_ms_us >= 1000){
		ms++;
		last_ms_us = us;
	}
}

void bl_tick_init(uint32_t frequency)
{
	sys_tick_frequency = frequency;
	systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);

	/* Set up the systick to trigger every microsecond*/
	uint32_t mhz = sys_tick_frequency / 1000000;

	/* System clock is xMHz, so 1/x usec per "tick" there and we want to tick
	 * res * usec, so that means a counter of (res * x) -1
	 */
	systick_set_reload((res * mhz) - 1);

	systick_clear();
	systick_interrupt_enable();
	systick_counter_enable();
}


void bl_set_us_timer(uint32_t *timer)
{
	*timer = us;
}

uint32_t bl_get_us_timer_elapsed(uint32_t timer)
{
	return (us - timer);
}

void bl_set_ms_timer(uint32_t *timer)
{
	*timer = ms;
}

uint32_t bl_get_ms_timer_elapsed(uint32_t timer)
{
	return (ms - timer);
}

int bl_set_resolution(uint32_t usec)
{
	if (MAX_TIMER_RES_US % usec ||
		usec < MIN_TIMER_RES_US ||
		usec > MAX_TIMER_RES_US){
		return -1;
	}
	/* Set the new resolution */
	res = usec;
	uint32_t mhz = sys_tick_frequency / 1000000;
	systick_set_reload((res * mhz) - 1);
	/* Add the remaining time to us and clear the counter */
	uint32_t ticks_left = systick_get_value();
	us += ticks_left * mhz;
	/* Not sure if we need to disable interrupt here */
	systick_interrupt_disable();
	systick_clear();
	systick_interrupt_enable();
	return 0;
}

uint32_t bl_get_resolution(void)
{
	return res;
}
