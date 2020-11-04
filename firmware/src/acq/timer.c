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

#include "timer.h"
#include "rcc.h"

#include "common/util.h"
#include "../led.h"

#include <libopencm3/stm32/timer.h>



typedef struct
{
	uint32_t prescale;
	uint32_t period;
} bl_acq_timer_config_t;

struct bl_acq_timer_s
{
	const uint32_t rcc_unit;
	const uint32_t base;
	uint32_t       frequency;
	unsigned       enable;

	bl_acq_timer_config_t config;
};

static bl_acq_timer_t bl_acq__tim1 =
{
	.rcc_unit  = RCC_TIM1,
	.base      = TIM1,
	.enable    = 0,
};

bl_acq_timer_t *bl_acq_tim1 = &bl_acq__tim1;

static bl_acq_timer_t *bl_acq_timer[] =
{
	&bl_acq__tim1,
};


static uint32_t bl_acq_timer__bus_freq = 0;

void bl_acq_timer_init(uint32_t bus_freq)
{
	bl_acq_timer__bus_freq = bus_freq;
}

#define DIV_NEAREST(_num, _den) (((_num) + (_den) / 2) / (_den))

enum bl_error bl_acq_timer_configure(bl_acq_timer_t *timer, uint32_t frequency)
{
	if (timer->enable > 0) {
		return BL_ERROR_ACTIVE_ACQUISITION;
	}

	if ((frequency == 0) || (frequency > bl_acq_timer__bus_freq)) {
		return BL_ERROR_TIMER_BAD_FREQUENCY;
	}

	uint32_t ticks_per_sample = DIV_NEAREST(
			bl_acq_timer__bus_freq, frequency);

	/* Find prime factors and increase prescale. */
	uint32_t prescale = ((ticks_per_sample & 1) ? 1 : 2);
	uint32_t ticks   = ticks_per_sample / prescale;
	for (unsigned i = 3; (i * i) < ticks; i += 2) {
		if ((ticks % i) == 0) {
			if ((prescale * i) > 65536) {
				break;
			}

			ticks    /= i;
			prescale *= i;
		}
	}

	/* It's very unlikely that ticks_per_sample is a prime above 65536,
	 * however we handle that via division here. */
	while (ticks > 65536) {
		ticks = DIV_NEAREST(ticks, 2);
		prescale *= 2;
	}

	timer->config.prescale = prescale;
	timer->config.period   = ticks;
	return BL_ERROR_NONE;
}

void bl_acq_timer_enable(bl_acq_timer_t *timer)
{
	if (!bl_acq__rcc_enable_ref(timer->rcc_unit, &timer->enable)) {
		/* Timer was already enabled. */
		return;
	}

	timer_set_prescaler(timer->base, (timer->config.prescale - 1));
	timer_set_period(timer->base, (timer->config.period - 1));

	timer->frequency = bl_acq_timer__bus_freq / (timer->config.prescale *
			timer->config.period);

	/* TODO: Enable specific triggers rather than just OC1 */

	/* Timer will trigger TRGO every OC1 match (once per period). */
	timer_set_oc_mode(timer->base, TIM_OC1, TIM_OCM_TOGGLE);
	timer_set_master_mode(timer->base, TIM_CR2_MMS_COMPARE_OC1REF);
	timer_enable_oc_output(timer->base, TIM_OC1);
}

void bl_acq_timer_disable(bl_acq_timer_t *timer)
{
	bl_acq__rcc_disable_ref(timer->rcc_unit, &timer->enable);
}

void bl_acq_timer_start(const bl_acq_timer_t *timer)
{
	timer_enable_counter(timer->base);
}

void bl_acq_timer_stop(const bl_acq_timer_t *timer)
{
	timer_disable_counter(timer->base);
}

void bl_acq_timer_start_all(void)
{
	for (unsigned i = 0; i < BL_ARRAY_LEN(bl_acq_timer); i++) {
		bl_acq_timer_t *timer = bl_acq_timer[i];
		if (timer->enable > 0) {
			bl_acq_timer_start(timer);
		}
	}
}

void bl_acq_timer_stop_all(void)
{
	for (unsigned i = 0; i < BL_ARRAY_LEN(bl_acq_timer); i++) {
		bl_acq_timer_t *timer = bl_acq_timer[i];
		if (timer->enable > 0) {
			bl_acq_timer_stop(timer);
		}
	}
}

