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

#ifndef BL_ACQ_DAC_H
#define BL_ACQ_DAC_H

#include <stdint.h>

#include "common/error.h"

typedef struct bl_acq_dac_s bl_acq_dac_t;

extern bl_acq_dac_t *bl_acq_dac3;
extern bl_acq_dac_t *bl_acq_dac4;

void bl_acq_dac_init(uint32_t bus_freq);

void bl_acq_dac_calibrate(bl_acq_dac_t *dac);

enum bl_error bl_acq_dac_channel_configure(bl_acq_dac_t *dac,
		uint8_t channel, uint16_t offset);

void bl_acq_dac_enable(bl_acq_dac_t *dac);
void bl_acq_dac_disable(bl_acq_dac_t *dac);

#endif

