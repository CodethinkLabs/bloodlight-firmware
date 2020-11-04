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

#ifndef BL_ACQ_RCC_H
#define BL_ACQ_RCC_H

#include <stdbool.h>
#include <stdint.h>

#include <libopencm3/stm32/rcc.h>

static inline bool bl_acq__rcc_enable_ref(uint32_t rcc_unit, unsigned *ref)
{
	*ref += 1;

	if (*ref == 1) {
		rcc_periph_clock_enable(rcc_unit);
		return true;
	}

	return false;
}

static inline bool bl_acq__rcc_disable_ref(uint32_t rcc_unit, unsigned *ref)
{
	if (*ref == 0) {
		return false;
	}

	*ref -= 1;
	if (*ref == 0) {
		rcc_periph_clock_disable(rcc_unit);
		return true;
	}

	return false;
}

#endif

