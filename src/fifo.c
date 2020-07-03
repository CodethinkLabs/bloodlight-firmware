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

#include <string.h>

#include "fifo.h"

#if (FIFO_SIZE > 0xFFFF)
#error "FIFO sizes above 64k not supported."
#endif

struct fifo_channel_priv
{
	struct fifo_channel config;

	uint8_t          *ptr;
	uint16_t          size;
	volatile uint16_t read, write;
	volatile bool     full, overflow;
};

static struct fifo_channel_priv priv_fifo_channels[FIFO_CHANNEL_MAX];
static uint8_t fifo[FIFO_SIZE];
static uint8_t sample_size; // Expect 2 or 4

bool fifo_config(unsigned count, struct fifo_channel *channel, uint8_t samp_size)
{
	if ((count == 0) || (count > FIFO_CHANNEL_MAX)) {
		return false;
	}

	if (!channel) return false;

	for (unsigned i = 0; i < count; i++) {
		if (samp_size > sizeof(uint32_t)) {
			return false;
		}
	}
	sample_size = samp_size;
	uint16_t  size = FIFO_SIZE / count;
	for (unsigned i = 0, offset = 0; i < count; i++, offset += size) {
		struct fifo_channel_priv *chan = &priv_fifo_channels[i];
		chan->config   = channel[i];
		chan->ptr      = &fifo[offset];
		chan->read     = 0;
		chan->write    = chan->read;
		chan->size     = size - (size % sample_size);
		chan->full     = false;
		chan->overflow = false;
	}

	return true;
}

uint8_t fifo_get_sample_size(void)
{
	return sample_size;
}

bool fifo_overflow_check(unsigned channel)
{
	if (channel >= FIFO_CHANNEL_MAX) return false;
	return priv_fifo_channels[channel].overflow;
}

void fifo_overflow_clear(unsigned channel)
{
	if (channel >= FIFO_CHANNEL_MAX) return;
	priv_fifo_channels[channel].overflow = false;
}

void fifo_write(unsigned channel, uint32_t sample)
{
	if (channel >= FIFO_CHANNEL_MAX) return;

	struct fifo_channel_priv *chan   = &priv_fifo_channels[channel];
	struct fifo_channel      *config = &chan->config;

	// Can't commit sample as FIFO is full.
	if (chan->full) {
		chan->overflow = true;
		return;
	}

	if (config->saturate && (sample < config->offset)) {
		sample = 0;
	} else {
		sample -= config->offset;
	}

#ifdef FIFO_SAMPLE_ROUNDING
	sample += (1U << (config->shift - 1));
#endif
	sample >>= config->shift;

	if (config->saturate && (sample_size < sizeof(uint32_t))) {
		uint32_t max_size = (1U << (sample_size * 8)) - 1;
		if (sample > max_size) sample = max_size;
	}

	for (unsigned i = 0; i < sample_size; i++) {
		/* We assume little endian here. */
		chan->ptr[chan->write++] = sample & 0xFF;
		sample >>= 8;
	}

	if (chan->write >= chan->size) {
		chan->write = 0;
	}
	if (chan->write == chan->read) {
		chan->full = true;
	}
}

uint16_t fifo_used(unsigned channel)
{
	struct fifo_channel_priv *chan   = &priv_fifo_channels[channel];
	struct fifo_channel      *config = &chan->config;

	if (chan->full) return chan->size;

	if (chan->write >= chan->read) {
		return (chan->write - chan->read);
	}

	return (chan->size - chan->read) + chan->write;
}

bool fifo_read(unsigned channel, void *sample)
{
	if (channel >= FIFO_CHANNEL_MAX) return false;

	struct fifo_channel_priv *chan   = &priv_fifo_channels[channel];
	struct fifo_channel      *config = &chan->config;

	if (!chan->full && (chan->read == chan->write)) {
		return false;
	}

	memcpy(sample, &chan->ptr[chan->read], sample_size);
	chan->read += sample_size;

	if (chan->read == chan->size) {
		chan->read = 0;
	}
	chan->full = false;
	return true;
}
