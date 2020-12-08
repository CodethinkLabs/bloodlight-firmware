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
#include "common/led.h"

#include "util.h"
#include "device.h"
#include "data-cal.h"
#include "main-menu.h"

/** Number of seconds to ignore at the start of calibration. */
#define DATA_CAL_IGNORE_SECONDS 2

/** Channel tracking. */
struct channel_data {
	uint32_t sample_min;   /**< Lowest sample seen for channel. */
	uint32_t sample_max;   /**< Highest sample seen for channel. */
	uint64_t sample_count; /**< Number of samples. */
	uint16_t src;          /**< Source of the channel. */
};

/** Calibration context. */
struct data_cal_ctx {
	struct channel_data *channel; /**< Array of channels' info. */
	unsigned count;               /**< Number of entries in channels. */

	unsigned frequency;           /**< Acquisition frequency. */
	uint32_t src_mask;            /**< Acquisition source mask. */
};

/**
 * Get the gain and offset for a given source.
 *
 * \param[in]  sample_min                Minumum sample value recorded.
 * \param[in]  sample_max                Maximum sample value recorded.
 * \param[in]  hw_scale                  The scale of hardware configs
 * \param[in]  source                    Acq source for this channel.
 * \param[out] out_source_opamp_gain     Returns opamp gain to set for source.
 * \param[out] out_source_opamp_offset   Returns opamp offset to set for source.
 */
static void data_cal__calibrate_analog(
		uint32_t  sample_min,
		uint32_t  sample_max,
		uint32_t  hw_scale,
		enum bl_acq_source source,
		uint32_t *out_source_opamp_gain,
		uint32_t *out_source_opamp_offset)
{
	uint32_t opamp_gain;
	uint32_t opamp_offset;

	uint32_t sample_range, sample_max_range;
	uint32_t margin;
	uint32_t sample_mid;
	uint32_t sample_pos, sample_neg;
	uint32_t source_range;

	const struct device_source_cap *source_cap = device_get_source_cap(source);

	/* Add some margin to calculations. */
	sample_range = sample_max - sample_min;
	margin = sample_range / 10;

	sample_max_range = 0xFFF;
	sample_max_range <<= hw_scale;

	sample_min = (margin > sample_min ? 0 : sample_min - margin);

	sample_max += margin;
	if (sample_max > sample_max_range) {
		sample_max = sample_max_range;
	}

	sample_mid = (sample_min + sample_max + 1) / 2;

	if (source_cap->opamp_offset == true) {
		opamp_offset = sample_mid;
		opamp_offset >>= hw_scale;

		/* opamp_offset is inverted. */
		opamp_offset = 4095 - opamp_offset;
	} else {
		/* opamp_offset is an offset integer so 2048 is zero. */
		opamp_offset = 2048;
	}

	sample_pos = sample_max - sample_mid;
	sample_neg = sample_mid - sample_min;

	source_range = max_u32(sample_pos, sample_neg);
	source_range >>= hw_scale;

	opamp_gain = 1;
	for (unsigned i = 0; i < source_cap->opamp_gain_count; i++) {
		if ((source_range * source_cap->opamp_gain[i]) > 2047) {
			break;
		}
		opamp_gain = source_cap->opamp_gain[i];
	}

	*out_source_opamp_gain   = opamp_gain;
	*out_source_opamp_offset = opamp_offset;
}

/**
 * Get the shift and offset for a given channel.
 *
 * \param[in]  sample_min                Minumum sample value recorded.
 * \param[in]  sample_max                Maximum sample value recorded.
 * \param[in]  opamp_gain                opamp gain for source.
 * \param[in]  hw_scale                  The scale of hardware configs
 * \param[in]  source                    Acq source for this channel.
 * \param[out] out_channel_shift         Returns shift to set for channel.
 * \param[out] out_channel_offset        Returns offset to set for channel.
 */
static void data_cal__calibrate_digital(
		uint32_t  sample_min,
		uint32_t  sample_max,
		uint32_t  opamp_gain,
		uint32_t  hw_scale,
		enum bl_acq_source source,
		uint32_t *out_channel_shift,
		uint32_t *out_channel_offset)
{
	uint32_t ch_shift;
	uint32_t ch_offset;

	uint32_t sample_range, sample_max_range;
	uint32_t margin;
	uint32_t sample_mid;
	uint32_t sample_pos, sample_neg;
	uint32_t target_max;

	uint32_t source_sw_oversample = main_menu_config_get_source_sw_oversample(source);
	const struct device_source_cap *source_cap = device_get_source_cap(source);

	/* Add some margin to calculations. */
	sample_range = sample_max - sample_min;
	margin = sample_range / 10;

	sample_max_range = 0xFFF;
	sample_max_range <<= hw_scale;
	sample_max_range *= source_sw_oversample;

	sample_min = (margin > sample_min ? 0 : sample_min - margin);

	sample_max += margin;
	if (sample_max > sample_max_range) {
		sample_max = sample_max_range;
	}

	sample_mid = (sample_min + sample_max + 1) / 2;

	sample_pos = sample_max - sample_mid;
	sample_neg = sample_mid - sample_min;

	target_max = sample_max;
	if (source_cap->opamp_offset == true) {
		uint32_t sample_mid_offset = 2048;
		sample_mid_offset <<= hw_scale;
		sample_mid_offset *= source_sw_oversample;

		target_max = sample_mid_offset + (sample_pos * opamp_gain);

		ch_offset = sample_mid_offset - (sample_neg * opamp_gain);
	} else {
		target_max = sample_max * opamp_gain;

		ch_offset = sample_min * opamp_gain;
	}

	ch_shift = 0;
	if (target_max < 65535) {
		ch_offset = 0;
	} else if ((target_max - ch_offset) < 65535) {
		ch_offset = (target_max - 65535) / 2;
		target_max -= ch_offset;
	} else {
		target_max -= ch_offset;

		while ((target_max >> ch_shift) > 65535)
			ch_shift++;

		/* Properly center waveform in shifted case. */
		uint32_t rem = 65535 - (target_max >> ch_shift);
		ch_offset -= (rem / 2) << ch_shift;
	}

	*out_channel_shift  = ch_shift;
	*out_channel_offset = ch_offset;
}

/**
 * Convert a data channel to an acquisition channel.
 *
 * \param[in]  channel  The channel to find the acquition for.
 * \return the channel's acquisition channel.
 */
static uint32_t data_cal__data_channel_to_acq_channel(
		unsigned channel)
{
	unsigned channel_mask = 0;
	enum bl_acq_flash_mode mode = main_menu_config_get_acq_emission_mode();

	switch (mode) {
	case BL_ACQ_FLASH:
		channel_mask = main_menu_config_get_led_mask();
		break;
	case BL_ACQ_CONTINUOUS:
		channel_mask = main_menu_config_get_source_mask();
		break;
	}

	for (unsigned i = 0; i < 32; i++) {
		if (channel_mask & (1u << i)) {
			if (channel == 0) {
				return i;
			}
			channel--;
		}
	}

	return UINT32_MAX;
}

/* Exported interface, documented in data-cal.h */
void data_cal_fini(void *pw)
{
	struct data_cal_ctx *ctx = pw;
	struct {
		uint32_t gain;
		uint32_t offset;
		bool     set;
	} source_opamp[BL_ACQ_SOURCE_MAX] = {{0, 0, false}};

	/* As there might be multiple channels sharing the
	 * same source, only the first channel is going to
	 * set the opamp gain/offset for the source
	 */
	for (unsigned i = 0; i < ctx->count; i++) {
		struct channel_data *c = &ctx->channel[i];
		enum bl_acq_source src;
		uint32_t channel;
		uint32_t channel_shift;
		uint32_t channel_offset;

		channel = data_cal__data_channel_to_acq_channel(i);
		if (channel == UINT32_MAX) {
			continue;
		}

		src = c->src;
		if (src >= BL_ACQ_SOURCE_MAX) {
			continue;
		}

		uint32_t hw_scale =
				main_menu_config_get_source_hw_oversample(src) -
				main_menu_config_get_source_hw_shift(src);

		if (!source_opamp[src].set) {
			source_opamp[src].set = true;
			data_cal__calibrate_analog(
				c->sample_min,
				c->sample_max,
				hw_scale,
				src,
				&source_opamp[src].gain,
				&source_opamp[src].offset);
		}

		data_cal__calibrate_digital(
				c->sample_min,
				c->sample_max,
				source_opamp[src].gain,
				hw_scale,
				src,
				&channel_shift,
				&channel_offset);

		fprintf(stderr, "Calibration: Channel %"PRIu32": "
				"Min: %"PRIu32", Max: %"PRIu32"\n",
				channel, c->sample_min, c->sample_max);

		main_menu_config_set_channel_shift(channel, channel_shift);
		main_menu_config_set_channel_offset(channel, channel_offset);

		main_menu_config_set_source_opamp_gain(src, source_opamp[src].gain);
		main_menu_config_set_source_opamp_offset(src, source_opamp[src].offset);
	}

	free(ctx->channel);
	free(ctx);
}

/**
 * Allocate the channel array.
 *
 * \param[in]   mask      The mask of channels.
 * \param[in]   count     The number of channels.
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
		unsigned acq_channel = data_cal__data_channel_to_acq_channel(i);
		channel[i].sample_min = 0xFFFFFFFFu;
		channel[i].sample_max = 0x00000000u;
		channel[i].sample_count = 0;
		channel[i].src = device_get_channel_source(acq_channel);
	}

	return channel;
}

/* Exported interface, documented in data-cal.h */
void *data_cal_init(
		unsigned frequency,
		unsigned channel_mask)
{
	struct data_cal_ctx *ctx;

	ctx = calloc(1, sizeof(*ctx));
	if (ctx == NULL) {
		return NULL;
	}

	unsigned channel_count = util_bit_count(channel_mask);

	ctx->channel = data_cal__init_channel_array(channel_count);
	if (ctx->channel == NULL) {
		free(ctx);
		return NULL;
	}

	ctx->frequency = frequency;
	ctx->src_mask = main_menu_config_get_source_mask();
	ctx->count = channel_count;
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

	c->sample_count++;
	if (c->sample_count < DATA_CAL_IGNORE_SECONDS * ctx->frequency) {
		/* Allow signal to stabilise; ignore early samples. */
		return sample;
	}

	if (sample < c->sample_min) {
		c->sample_min = sample;
	}
	if (sample > c->sample_max) {
		c->sample_max = sample;
	}

	return sample;
}
