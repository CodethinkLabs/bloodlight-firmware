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
 * \brief Implementation of the derivative filter.
 *
 * This module calculates the first derivative of a channel.  It can be
 * applied to its own result, to get the second derivative.
 */

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "derivative.h"
#include "util.h"

/** Channel tracking. */
struct channel_data {
	uint32_t prev; /**< Previous sample. */
};

/** Averaging filter context. */
struct derivative_ctx {
	struct channel_data *channel; /**< Array of channel's data. */
	unsigned count;               /**< Number of entries in array. */
};

/* Exported interface, documented in data-avg.h */
void derivative_fini(void *pw)
{
	struct derivative_ctx *ctx = pw;

	for (unsigned i = 0; i < ctx->count; i++) {
		ctx->channel[i].prev = 0;
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
static struct channel_data *derivative__init_channel_array(
		unsigned count)
{
	struct channel_data *channel;

	channel = calloc(sizeof(*channel), count);
	if (channel == NULL) {
		return NULL;
	}

	for (unsigned i = 0; i < count; i++) {
		channel[i].prev = INT32_MAX;
	}

	return channel;
}

/* Exported interface, documented in data-avg.h */
void *derivative_init(
		unsigned frequency,
		unsigned channels,
		uint32_t src_mask)
{
	struct derivative_ctx *ctx;

	BV_UNUSED(frequency);
	BV_UNUSED(src_mask);

	ctx = calloc(1, sizeof(*ctx));
	if (ctx == NULL) {
		return NULL;
	}

	ctx->channel = derivative__init_channel_array(channels);
	if (ctx->channel == NULL) {
		free(ctx);
		return NULL;
	}

	ctx->count = channels;
	return ctx;
}

/* Exported interface, documented in data-avg.h */
uint32_t derivative_proc(
		void *pw,
		unsigned channel,
		uint32_t sample)
{
	struct derivative_ctx *ctx = pw;
	struct channel_data *c;
	uint32_t value;

	assert(channel < ctx->count);

	c = &ctx->channel[channel];

	value = INT32_MAX + sample - c->prev;
	c->prev = sample;

	return value;
}
