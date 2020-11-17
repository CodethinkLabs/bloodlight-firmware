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
 * \brief Implementation of the data module.
 *
 * This provides data filtering functionality.
 */

#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>

#include "common/msg.h"

#include "host/common/fifo.h"

#include "data.h"
#include "util.h"
#include "graph.h"
#include "data-avg.h"
#include "data-cal.h"
#include "main-menu.h"
#include "derivative.h"
#include "data-invert.h"

/** Number of samples to store channel masks for. */
#define DATA_MASKS_COUNT (1 << 7)

/** Mask into sample_masks array. */
#define DATA_MASKS_MASK  (DATA_MASKS_COUNT - 1)

/** Data filter details. */
struct data_filter {
	void *ctx; /**< The filter context. */

	/** The filter finalisation callback. */
	void (*fini)(
			void *pw);

	/** The filter's sample processing function. */
	uint32_t (*proc)(
			void *pw,
			unsigned channel,
			uint32_t sample);
};

/** Data channel */
struct data_channel {
	unsigned index; /**< Acquisition channel index. */

	struct fifo *samples; /**< FIFO for the channel's samples. */

	uint64_t sample_count; /**< Channel's sample count. */
};

/** Data module global data. */
static struct {
	/** Whether the data module has been initialised. */
	bool enabled;

	/** Acquisition channel to data channel. */
	unsigned mapping[sizeof(unsigned) * CHAR_BIT];

	/** Array of data channels. */
	struct data_channel *channel;
	unsigned channel_count; /**< Number of channels in channel array. */
	unsigned channel_mask;

	unsigned sample_masks[DATA_MASKS_COUNT];

	/** Array of registered filters. */
	struct data_filter *filter;

	/** Number of entries in \ref filter. */
	unsigned count;
} data_g;

/**
 * Destroy channels.
 */
static void data__destroy_channels(void)
{
	if (data_g.channel != NULL) {
		for (unsigned i = 0; i < data_g.channel_count; i++) {
			fifo_destroy(data_g.channel[i].samples);
		}
		free(data_g.channel);
		data_g.channel = NULL;
	}

	data_g.channel_count = 0;
	data_g.channel_mask = 0;
}

/**
 * Create channels.
 *
 * \param[in]  channel_mask  Acquition channel mask.
 * \return true on success or false on error.
 */
static bool data__create_channels(unsigned channel_mask)
{
	unsigned channels = util_bit_count(channel_mask);
	unsigned n;

	data_g.channel = calloc(channels, sizeof(*data_g.channel));
	if (data_g.channel == NULL) {
		return false;
	}
	data_g.channel_count = channels;
	data_g.channel_mask = channel_mask;

	n = 0;
	for (unsigned i = 0; i < sizeof(channel_mask) * CHAR_BIT; i++) {
		if (channel_mask & (1u << i)) {
			data_g.channel[n].index = i;
			data_g.channel[n].samples = fifo_create(
					MSG_SAMPLE_DATA16_MAX * 4,
					sizeof(uint32_t));
			if (data_g.channel[n].samples == NULL) {
				goto error;
			}

			data_g.mapping[i] = n++;
		} else {
			data_g.mapping[i] = UINT_MAX;
			continue;
		}
	}

	return true;
error:
	data__destroy_channels();
	return false;
}

/**
 * Filter and graph a sample.
 *
 * \param[in]  channel  The channel that the sample is for.
 * \param[in]  sample   The sample to filter.
 * \return true on success, or false on error.
 */
static bool data__process_sample(
		unsigned channel,
		uint32_t sample)
{
	for (unsigned i = 0; i < data_g.count; i++) {
		struct data_filter *filter = &data_g.filter[i];
		sample = filter->proc(filter->ctx, channel, sample);
	}

	if (!graph_data_add(channel, sample - INT32_MAX)) {
		return false;
	}

	return true;
}

/**
 * Handle a sample.
 *
 * This stashes the sample in the channel's fifo, and once all channels
 * have got a given sample, the sample is read out of all the channel fifos
 * and processed.
 *
 * \param[in]  acq_channel  Acqusition channel for sample.
 * \param[in]  sample       Sample to handle.
 * \return true on success, false on error.
 */
static bool data__handle_sample(
		unsigned acq_channel,
		uint32_t sample)
{
	unsigned data_channel = data_g.mapping[acq_channel];
	struct data_channel *channel = &data_g.channel[data_channel];
	unsigned index = channel->sample_count & DATA_MASKS_MASK;

	if (data_g.sample_masks[index] & (1u << acq_channel)) {
		fprintf(stderr, "Data error: Channel %u sample overrun\n",
				acq_channel);
		return false;
	}

	if (!fifo_write(channel->samples, &sample)) {
		fprintf(stderr, "Data error: Channel %u fifo overrun\n",
				acq_channel);
		return false;
	}
	channel->sample_count++;

	data_g.sample_masks[index] |= (1u << acq_channel);
	if (data_g.sample_masks[index] == data_g.channel_mask) {

		for (unsigned i = 0; i < data_g.channel_count; i++) {
			uint32_t ready_sample;
			if (!fifo_read(data_g.channel[i].samples,
					&ready_sample)) {
				fprintf(stderr, "Data error: "
						"Channel %u fifo underrun\n",
						i);
				return false;
			}

			if (!data__process_sample(i, ready_sample)) {
				return false;
			}
		}
		data_g.sample_masks[index] = 0;
	}

	return true;
}

/* Exported interface, documented in data.h */
bool data_handle_msg_u16(const bl_msg_sample_data_t *msg)
{
	unsigned acq_channel = msg->channel;

	if (data_g.enabled == false) {
		return true;
	}

	assert(msg->type == BL_MSG_SAMPLE_DATA16);

	for (unsigned i = 0; i < msg->count; i++) {
		if (!data__handle_sample(
				acq_channel,
				msg->data16[i])) {
			return false;
		}
	}

	return true;
}

/* Exported interface, documented in data.h */
bool data_handle_msg_u32(const bl_msg_sample_data_t *msg)
{
	unsigned acq_channel = msg->channel;

	if (data_g.enabled == false) {
		return true;
	}

	assert(msg->type == BL_MSG_SAMPLE_DATA32);

	for (unsigned i = 0; i < msg->count; i++) {
		if (!data__handle_sample(
				acq_channel,
				msg->data32[i])) {
			return false;
		}
	}

	return true;
}

/* Exported interface, documented in data.h */
void data_finish(void)
{
	data_g.enabled = false;

	for (unsigned i = 0; i < data_g.count; i++) {
		data_g.filter[i].fini(data_g.filter[i].ctx);
	}

	free(data_g.filter);
	data_g.filter = NULL;
	data_g.count = 0;

	graph_fini();

	data__destroy_channels();
	memset(data_g.sample_masks, 0, sizeof(data_g.sample_masks));
}

/**
 * Increase filter array allocation, and return new filter slot.
 *
 * Does not increase the filter count, so the caller must do that.
 *
 * \return the new filter allocation, or NULL on error.
 */
static struct data_filter *data__allocate_filter(void)
{
	struct data_filter *filter;
	unsigned count = data_g.count;

	filter = realloc(data_g.filter, sizeof(*filter) * (count + 1));
	if (filter == NULL) {
		return NULL;
	}

	data_g.filter = filter;

	filter += count;
	memset(filter, 0, sizeof(*filter));

	return filter;
}

/**
 * Resister the calibration filter, if enabled.
 *
 * \param[in]  frequency     The sampling frequency.
 * \param[in]  channels      The number of channels.
 * \param[in]  channel_mask  Mask of enabled channels.
 * \return true on success, or false on error.
 */
static bool data__register_calibrate(
		unsigned frequency,
		unsigned channels,
		uint32_t channel_mask)
{
	struct data_filter *filter;

	filter = data__allocate_filter();
	if (filter == NULL) {
		return false;
	}

	filter->ctx = data_cal_init(frequency, channels, channel_mask);
	if (filter->ctx == NULL) {
		/* No need to free the filter, it's already owned by the
		 * global state. */
		return false;
	}

	filter->fini = data_cal_fini;
	filter->proc = data_cal_proc;

	data_g.count++;
	return true;
}

/**
 * Resister the sample value recovery filter, if required.
 *
 * This revovers the true values for calibrated channels.
 *
 * \param[in]  frequency     The sampling frequency.
 * \param[in]  channels      The number of channels.
 * \param[in]  channel_mask  Mask of enabled channels.
 * \return true on success, or false on error.
 */
static bool data__register_recover(
		unsigned frequency,
		unsigned channels,
		uint32_t channel_mask)
{
	BV_UNUSED(frequency);
	BV_UNUSED(channels);
	BV_UNUSED(channel_mask);

	return true;
}

/**
 * Resister the normalising filter, if enabled.
 *
 * \param[in]  frequency     The sampling frequency.
 * \param[in]  channels      The number of channels.
 * \param[in]  channel_mask  Mask of enabled channels.
 * \return true on success, or false on error.
 */
static bool data__register_normalise(
		unsigned frequency,
		unsigned channels,
		uint32_t channel_mask)
{
	struct data_filter *filter;
	struct data_avg_config config = {
		.filter_freq = 1024 * main_menu_config_get_filter_normalise_frequency(),
		.normalise   = true,
	};

	filter = data__allocate_filter();
	if (filter == NULL) {
		return false;
	}

	filter->ctx = data_avg_init(&config, frequency, channels, channel_mask);
	if (filter->ctx == NULL) {
		/* No need to free the filter, it's already owned by the
		 * global state. */
		return false;
	}

	filter->fini = data_avg_fini;
	filter->proc = data_avg_proc;

	data_g.count++;
	return true;
}

/**
 * Resister the inverting filter.
 *
 * \param[in]  frequency  The sampling frequency.
 * \param[in]  channels   The number of channels.
 * \param[in]  channel_mask  Mask of enabled channels.
 * \return true on success, or false on error.
 */
static bool data__register_invert(
		unsigned frequency,
		unsigned channels,
		uint32_t channel_mask)
{
	bool enabled = false;
	struct data_filter *filter;
	struct data_invert_config config = { 0 };

	for (unsigned i = 0; i < BL_ACQ_SOURCE_MAX; i++) {
		config.invert[i] = main_menu_config_get_channel_inverted(i);
		if (config.invert[i]) {
			enabled = true;
		}
	}

	if (!enabled) {
		/* No inversion required. */
		return true;
	}

	filter = data__allocate_filter();
	if (filter == NULL) {
		return false;
	}

	filter->ctx = data_invert_init(&config, frequency,
			channels, channel_mask);
	if (filter->ctx == NULL) {
		/* No need to free the filter, it's already owned by the
		 * global state. */
		return false;
	}

	filter->fini = data_invert_fini;
	filter->proc = data_invert_proc;

	data_g.count++;
	return true;
}

/**
 * Resister the AC noise suppression filter, if enabled.
 *
 * \param[in]  frequency     The sampling frequency.
 * \param[in]  channels      The number of channels.
 * \param[in]  channel_mask  Mask of enabled channels.
 * \return true on success, or false on error.
 */
static bool data__register_ac_denoise(
		unsigned frequency,
		unsigned channels,
		uint32_t channel_mask)
{
	struct data_filter *filter;
	struct data_avg_config config = {
		.filter_freq = (frequency / main_menu_config_get_filter_ac_denoise_frequency()) * 1024,
		.normalise   = false,
	};

	filter = data__allocate_filter();
	if (filter == NULL) {
		return false;
	}

	filter->ctx = data_avg_init(&config, frequency, channels, channel_mask);
	if (filter->ctx == NULL) {
		/* No need to free the filter, it's already owned by the
		 * global state. */
		return false;
	}

	filter->fini = data_avg_fini;
	filter->proc = data_avg_proc;

	data_g.count++;
	return true;
}

/**
 * Resister the derivative filter.
 *
 * \param[in]  frequency     The sampling frequency.
 * \param[in]  channels      The number of channels.
 * \param[in]  channel_mask  Mask of enabled channels.
 * \return true on success, or false on error.
 */
static bool data__register_derivative(
		unsigned frequency,
		unsigned channels,
		uint32_t channel_mask)
{
	struct data_filter *filter;

	filter = data__allocate_filter();
	if (filter == NULL) {
		return false;
	}

	filter->ctx = derivative_init(frequency, channels, channel_mask);
	if (filter->ctx == NULL) {
		/* No need to free the filter, it's already owned by the
		 * global state. */
		return false;
	}

	filter->fini = derivative_fini;
	filter->proc = derivative_proc;

	data_g.count++;
	return true;
}

/**
 * Initialise the data context.
 *
 * \param[in]  calibrate     Whether this is a calibration acquisition.
 * \param[in]  frequency     The sampling frequency.
 * \param[in]  channels      The number of channels.
 * \param[in]  channel_mask  Mask of enabled channels.
 * \return true on success, or false on error.
 */
static bool data__register_filters(
		bool calibrate,
		unsigned frequency,
		unsigned channels,
		uint32_t channel_mask)
{
	if (calibrate) {
		if (!data__register_calibrate(
				frequency, channels, channel_mask)) {
			return false;
		}
	} else {
		if (!data__register_recover(
				frequency, channels, channel_mask)) {
			return false;
		}
	}


	if (!data__register_invert(frequency, channels, channel_mask)) {
		return false;
	}

	if (main_menu_config_get_filter_normalise_enabled() &&
			!data__register_normalise(
					frequency, channels, channel_mask)) {
		return false;
	}

	if (main_menu_config_get_filter_ac_denoise_enabled() &&
			!data__register_ac_denoise(
					frequency, channels, channel_mask)) {
		return false;
	}

	if (main_menu_config_get_derivative_mode() > BV_DERIVATIVE_NONE &&
			!data__register_derivative(
					frequency, channels, channel_mask)) {
		return false;
	}

	if (main_menu_config_get_derivative_mode() > BV_DERIVATIVE_FIRST &&
			!data__register_derivative(
					frequency, channels, channel_mask)) {
		return false;
	}

	return true;
}

/* Exported interface, documented in data.h */
bool data_start(bool calibrate, unsigned frequency, unsigned channel_mask)
{
	unsigned channels = util_bit_count(channel_mask);

	assert(data_g.enabled == false);

	if (!data__create_channels(channel_mask)) {
		return false;
	}

	if (!graph_init()) {
		data_finish();
		return false;
	}

	if (!data__register_filters(calibrate, frequency,
			channels, channel_mask)) {
		data_finish();
		return false;
	}

	for (unsigned i = 0; i < sizeof(channel_mask) * CHAR_BIT; i++) {
		if (data_g.mapping[i] == UINT_MAX) {
			continue;
		}
		if (!graph_create(data_g.mapping[i], frequency, i)) {
			data_finish();
			return false;
		}
	}

	data_g.enabled = true;

	return true;
}
