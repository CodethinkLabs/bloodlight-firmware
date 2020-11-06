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
 * \brief Implementation of device 16-bit value calibration.
 *
 * This module works out the correct channel settings for 16-bit acquisitions.
 */

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include "common/acq.h"

#include "util.h"
#include "data-cal.h"
#include "main-menu.h"

/** Channel tracking. */
struct channel_data {
	uint32_t sample_min; /**< Lowest sample seen for channel. */
	uint32_t sample_max; /**< Highest sample seen for channel. */
};

/** Calibration context. */
struct data_cal_ctx {
	struct channel_data *channel; /**< Array of channels' info. */
	unsigned count;               /**< Number of entries in channels. */

	uint32_t src_mask;            /**< Acquisition source mask. */
};

/**
 * Get the shift and offset for a given channel.
 *
 * \param[in]  bits        The number of bits to calibrate the value for.
 * \param[in]  sample_min  Minumum sample value recorded.
 * \param[in]  sample_max  Maximum sample value recorded.
 * \param[out] out_shift   Returns the value shift to set for the channel.
 * \param[out] out_offset  Returns the value offset to set for the channel.
 */
static void data_cal__calibrate_channel(
		uint8_t   bits,
		uint32_t  sample_min,
		uint32_t  sample_max,
		uint32_t *out_shift,
		uint32_t *out_offset)
{
	uint32_t max_range    = (1U << bits) - 1;
	uint32_t margin       = max_range / 4;
	uint32_t target_range = max_range - (margin * 2);
	uint32_t range        = sample_max - sample_min;
	uint32_t shift        = 0;
	uint32_t offset;

	while ((range >> shift) > target_range) {
		shift++;
	}

	offset = (margin < sample_min) ? (sample_min - margin) : 0;

	*out_shift  = shift;
	*out_offset = offset;
}

/**
 * Convert a data channel to an acquisition source.
 *
 * \param[in]  ctx      The calibration context.
 * \param[in]  channel  The channel to find the source for.
 * \return the channel's acquisition source.
 */
static enum bl_acq_source data_cal__channel_to_source(
		const struct data_cal_ctx *ctx,
		unsigned channel)
{
	for (unsigned i = 0; i < 32; i++) {
		if (ctx->src_mask & (1u << i)) {
			if (channel == 0) {
				return i;
			}
			channel--;
		}
	}

	return BL_ACQ_SOURCE_MAX;
}

/* Exported interface, documented in data-cal.h */
void data_cal_fini(void *pw)
{
	struct data_cal_ctx *ctx = pw;

	for (unsigned i = 0; i < ctx->count; i++) {
		struct channel_data *c = &ctx->channel[i];
		enum bl_acq_source src;
		uint32_t offset;
		uint32_t shift;

		data_cal__calibrate_channel(16,
				c->sample_min,
				c->sample_max,
				&shift, &offset);

		src = data_cal__channel_to_source(ctx, i);
		if (src >= BL_ACQ_SOURCE_MAX) {
			continue;
		}

		main_menu_config_set_channel_shift(i, shift);
		main_menu_config_set_channel_offset(i, offset);
	}

	free(ctx->channel);
	free(ctx);
}

/**
 * Allocate the channel array.
 *
 * \param[in]  count     The number of channels.
 */
static struct channel_data *data_cal__init_channel_array(
		unsigned count)
{
	struct channel_data *channel;

	channel = calloc(sizeof(*channel), count);
	if (channel == NULL) {
		return NULL;
	}

	for (unsigned i = 0; i < count; i++) {
		channel[i].sample_min = 0xFFFFFFFFu;
		channel[i].sample_max = 0x00000000u;
	}

	return channel;
}

/* Exported interface, documented in data-cal.h */
void *data_cal_init(
		unsigned frequency,
		unsigned channels,
		uint32_t src_mask)
{
	struct data_cal_ctx *ctx;

	BV_UNUSED(frequency);

	ctx = calloc(1, sizeof(*ctx));
	if (ctx == NULL) {
		return NULL;
	}

	ctx->channel = data_cal__init_channel_array(channels);
	if (ctx->channel == NULL) {
		free(ctx);
		return NULL;
	}

	ctx->src_mask = src_mask;
	ctx->count = channels;
	return ctx;
}

/* Exported interface, documented in data-cal.h */
uint32_t data_cal_proc(
		void *pw,
		unsigned channel,
		uint32_t sample)
{
	struct data_cal_ctx *ctx = pw;
	struct channel_data *c;

	assert(channel < ctx->count);

	c = &ctx->channel[channel];

	if (sample < c->sample_min) {
		c->sample_min = sample;
	}
	if (sample > c->sample_max) {
		c->sample_max = sample;
	}

	return sample;
}
