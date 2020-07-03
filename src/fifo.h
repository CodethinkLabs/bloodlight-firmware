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

#ifndef BL_FIFO_H
#define BL_FIFO_H

#include <stdbool.h>
#include <stdint.h>

#define FIFO_SIZE 4096
#define FIFO_CHANNEL_MAX 8

struct fifo_channel
{
	uint32_t offset;      // Amount to subtract from the sample before sending to
	                      // the host
	uint8_t  shift;       // Number of bits to "shift divide" the sample by
	bool     saturate;    // Whether to saturate or overflow
};

bool fifo_config(unsigned count, struct fifo_channel *channel, uint8_t samp_size);
uint8_t fifo_get_sample_size(void);
bool fifo_overflow_check(unsigned channel);
void fifo_overflow_clear(unsigned channel);

uint16_t fifo_used(unsigned channel);

void fifo_write(unsigned channel, uint32_t sample);
bool fifo_read(unsigned channel, void *sample);

#endif
