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

#ifndef BL_ACQ_CHANNEL_H
#define BL_ACQ_CHANNEL_H

#include <stdbool.h>
#include <stdint.h>

#include "common/error.h"
#include "common/channel.h"

#include "../acq.h"

#define BL_ACQ_CHANNEL_COUNT 19

#if (BL_ACQ_CHANNEL_COUNT > BL_CHANNEL_MAX)
#error "BL_ACQ_CHANNEL_COUNT must be less than BL_CHANNEL_MAX"
#endif

enum bl_error bl_acq_channel_configure(unsigned channel,
		enum bl_acq_source source,
		bool sample32, uint32_t sw_offset, uint8_t sw_shift);

void bl_acq_channel_enable(unsigned channel);
void bl_acq_channel_disable(unsigned channel);

bool bl_acq_channel_is_enabled(unsigned channel);

enum bl_acq_source bl_acq_channel_get_source(unsigned channel);

void bl_acq_channel_commit_sample(unsigned channel, uint32_t sample);

#endif
