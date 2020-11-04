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

#ifndef BL_ACQ_ADC_H
#define BL_ACQ_ADC_H

#include <stdbool.h>
#include <stdint.h>

#include "common/error.h"


typedef struct bl_acq_adc_group_s bl_acq_adc_group_t;

enum bl_error bl_acq_adc_group_configure(bl_acq_adc_group_t *adc_group,
		uint32_t clock, bool async, bool multi);
enum bl_error bl_acq_adc_group_configure_all(
		uint32_t clock, bool async, bool multi);



typedef struct bl_acq_adc_s bl_acq_adc_t;

extern bl_acq_adc_t *bl_acq_adc1;
extern bl_acq_adc_t *bl_acq_adc2;
extern bl_acq_adc_t *bl_acq_adc3;
extern bl_acq_adc_t *bl_acq_adc4;

#if (BL_REVISION >= 2)
extern bl_acq_adc_t *bl_acq_adc5;
#endif

void bl_acq_adc_calibrate(bl_acq_adc_t *adc);

enum bl_error bl_acq_adc_channel_configure(bl_acq_adc_t *adc,
	uint8_t channel, uint8_t source);
enum bl_error bl_acq_adc_configure(bl_acq_adc_t *adc,
		uint32_t frequency, uint32_t sw_oversample,
		uint8_t oversample, uint8_t shift);

void bl_acq_adc_flash_init(bl_acq_adc_t *adc, bool enable, bool master);

void bl_acq_adc_enable(bl_acq_adc_t *adc, uint32_t ccr_flag);
void bl_acq_adc_disable(bl_acq_adc_t *adc, uint32_t ccr_flag);

bl_acq_adc_group_t *bl_acq_adc_get_group(const bl_acq_adc_t *adc);

#include "timer.h"
bl_acq_timer_t     *bl_acq_adc_get_timer(const bl_acq_adc_t *adc);

#include "dma.h"
bl_acq_dma_t       *bl_acq_adc_get_dma(const bl_acq_adc_t *adc);

#endif

