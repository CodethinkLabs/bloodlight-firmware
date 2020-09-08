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

/**
 * \file
 * \brief Implementation of sample averaging.
 *
 * This module averages samples over a given range.
 */

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include "data-avg.h"
#include "util.h"

/** Channel tracking. */
struct channel_data {
	uint32_t *data; /**< Historical sample data buffer. */

	uint64_t  sum;         /**< Rolling sum of samples in data buffer. */
	unsigned  read;        /**< Read position in buffer. */
	unsigned  write;       /**< Write position in data buffer. */
	unsigned  capacity;    /**< Maximum capacity of data buffer, */
	unsigned  utilisation; /**< Amount of data buffer used. */
};

/** Averaging filter context. */
struct data_avg_ctx {
	struct channel_data *channel; /**< Array of channel's data. */
	unsigned count;               /**< Number of entries in array. */

	bool normalise; /**< Whether to apply normalisation. */
};

/* Exported interface, documented in data-avg.h */
void data_avg_fini(void *pw)
{
	struct data_avg_ctx *ctx = pw;

	for (unsigned i = 0; i < ctx->count; i++) {
		free(ctx->channel[i].data);
	}

	free(ctx->channel);
	free(ctx);
}

/**
 * Allocate the channel array.
 *
 * \param[in]  count     The number of channels.
 * \param[in]  capacity  The sample capacity for each channel.
 */
static struct channel_data *data_avg__init_channel_array(
		unsigned count,
		unsigned capacity)
{
	struct channel_data *channel;

	channel = calloc(sizeof(*channel), count);
	if (channel == NULL) {
		return NULL;
	}

	for (unsigned i = 0; i < count; i++) {
		size_t size = sizeof(*channel[i].data);

		channel[i].capacity = capacity;
		channel[i].data = malloc(capacity * size);
		if (channel[i].data == NULL) {
			goto error;
		}
	}

	return channel;

error:
	for (unsigned i = 0; i < count; i++) {
		free(channel[i].data);
	}
	free(channel);

	return NULL;
}

/* Exported interface, documented in data-avg.h */
void *data_avg_init(
		const struct data_avg_config *config,
		unsigned frequency,
		unsigned channels,
		uint32_t src_mask)
{
	struct data_avg_ctx *ctx;
	unsigned capacity;

	BV_UNUSED(src_mask);

	assert(config->filter_freq > 0);

	ctx = calloc(1, sizeof(*ctx));
	if (ctx == NULL) {
		return NULL;
	}

	capacity = frequency / (config->filter_freq / 1024.0);

	ctx->channel = data_avg__init_channel_array(channels, capacity);
	if (ctx->channel == NULL) {
		free(ctx);
		return NULL;
	}

	ctx->normalise = config->normalise;
	ctx->count = channels;
	return ctx;
}

/**
 * Advance a position pointer for a channel.
 *
 * \param[in]  c    A channel object.
 * \param[in]  pos  Current position pointer for channel.
 * \return new pointer position value.
 */
static inline unsigned data_avg__advance_pos(
		const struct channel_data *c,
		unsigned pos)
{
	pos++;
	return (pos >= c->capacity) ? 0 : pos;
}

/**
 * Get a average sample from a channel.
 *
 * \param[in]  c  A channel object.
 * \return Normalised sample.
 */
static inline uint32_t data_avg__get_average(
		const struct channel_data *c)
{
	return c->sum / c->utilisation;
}

/**
 * Get a normalised sample from a channel.
 *
 * \param[in]  c       A channel object.
 * \param[in]  sample  Sample to normalise.
 * \return Normalised sample.
 */
static inline uint32_t data_avg__get_normalised(
		const struct channel_data *c,
		uint32_t sample)
{
	return INT32_MAX + sample - data_avg__get_average(c);
}

/**
 * Write a sample to a channel buffer.
 *
 * \param[in]  c      Channel object.
 * \param[in]  value  Value to write to channel buffer.
 */
static inline void data_avg__add_sample(
		struct channel_data *c,
		uint32_t value)
{
	assert(c->utilisation < c->capacity);

	c->data[c->write] = value;
	c->write = data_avg__advance_pos(c, c->write);

	c->utilisation++;
	c->sum += value;
}

/**
 * Drop a sample from a channel buffer.
 *
 * \param[in]  c  Channel object.
 */
static inline void data_avg__drop_sample(
		struct channel_data *c)
{
	assert(c->utilisation > 0);

	uint32_t old = c->data[c->read];
	c->read = data_avg__advance_pos(c, c->read);

	c->utilisation--;
	c->sum -= old;
}

/* Exported interface, documented in data-avg.h */
uint32_t data_avg_proc(
		void *pw,
		unsigned channel,
		uint32_t sample)
{
	struct data_avg_ctx *ctx = pw;
	struct channel_data *c;
	uint32_t value;

	assert(channel < ctx->count);

	c = &ctx->channel[channel];

	data_avg__add_sample(c, sample);

	if (ctx->normalise) {
		value = data_avg__get_normalised(c, sample);
	} else {
		value = data_avg__get_average(c);
	}

	if (c->utilisation == c->capacity) {
		data_avg__drop_sample(c);
	}

	return value;
}
