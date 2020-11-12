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

#ifndef BL_ACQ_OPAMP_H
#define BL_ACQ_OPAMP_H

#include <stdint.h>

#include "common/error.h"


#define OPAMP_OFFSET_ZERO 2048


typedef struct bl_acq_opamp_s bl_acq_opamp_t;

#if (BL_REVISION == 1)
extern bl_acq_opamp_t *bl_acq_opamp2;
extern bl_acq_opamp_t *bl_acq_opamp4;
#else
extern bl_acq_opamp_t *bl_acq_opamp1;
extern bl_acq_opamp_t *bl_acq_opamp2;
extern bl_acq_opamp_t *bl_acq_opamp3;
extern bl_acq_opamp_t *bl_acq_opamp4;
extern bl_acq_opamp_t *bl_acq_opamp5;
extern bl_acq_opamp_t *bl_acq_opamp6;
#endif

void bl_acq_opamp_calibrate(bl_acq_opamp_t *opamp);

enum bl_error bl_acq_opamp_configure(bl_acq_opamp_t *opamp, uint8_t gain);

void bl_acq_opamp_enable(bl_acq_opamp_t *opamp);
void bl_acq_opamp_disable(bl_acq_opamp_t *opamp);

#if (BL_REVISION >= 2)
#include "dac.h"
bl_acq_dac_t *bl_acq_opamp_get_dac(const bl_acq_opamp_t *opamp,
		uint8_t *dac_channel);
#endif

#include "adc.h"
bl_acq_adc_t *bl_acq_opamp_get_adc(const bl_acq_opamp_t *opamp,
		uint8_t *adc_channel);

#endif

