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

#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>

#include "../../src/msg.h"

#include "data.h"
#include "util.h"
#include "graph.h"
#include "data-avg.h"
#include "main-menu.h"
#include "data-invert.h"

struct data_filter {
	void *ctx;

	void (*fini)(
			void *pw);

	uint32_t (*proc)(
			void *pw,
			unsigned channel,
			uint32_t sample);
};

/** Data module global data. */
static struct {
	/** Whether the data module has been initialised. */
	bool enabled;

	/** Acquisition channel to data channel. */
	unsigned mapping[sizeof(unsigned) * CHAR_BIT];

	/** Array of registered filters. */
	struct data_filter *filter;

	/** Number of entries in \ref filter. */
	unsigned count;
} data_g;

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

/* Exported interface, documented in data.h */
bool data_handle_msg_u16(const bl_msg_sample_data_t *msg)
{
	unsigned index = msg->channel;

	if (data_g.enabled == false) {
		return true;
	}

	assert(msg->type == BL_MSG_SAMPLE_DATA16);

	for (unsigned i = 0; i < msg->count; i++) {
		if (!data__process_sample(
				data_g.mapping[index],
				msg->data16[i])) {
			return false;
		}
	}

	return true;
}

/* Exported interface, documented in data.h */
bool data_handle_msg_u32(const bl_msg_sample_data_t *msg)
{
	unsigned index = msg->channel;

	if (data_g.enabled == false) {
		return true;
	}

	assert(msg->type == BL_MSG_SAMPLE_DATA32);

	for (unsigned i = 0; i < msg->count; i++) {
		if (!data__process_sample(
				data_g.mapping[index],
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
 * Resister the normalising filter, if enabled.
 *
 * \param[in]  frequency  The sampling frequency.
 * \param[in]  channels   The number of channels.
 * \param[in]  src_mask   Mask of enabled sources.
 * \return true on success, or false on error.
 */
static bool data__register_normalise(
		unsigned frequency,
		unsigned channels,
		uint32_t src_mask)
{
	struct data_filter *filter;
	struct data_avg_config config = {
		.filter_freq = 1024 * main_menu_conifg_get_filter_normalise_frequency(),
		.normalise   = true,
	};

	filter = data__allocate_filter();
	if (filter == NULL) {
		return false;
	}

	filter->ctx = data_avg_init(&config, frequency, channels, src_mask);
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
 * \param[in]  src_mask   Mask of enabled sources.
 * \return true on success, or false on error.
 */
static bool data__register_invert(
		unsigned frequency,
		unsigned channels,
		uint32_t src_mask)
{
	bool enabled = false;
	struct data_filter *filter;
	struct data_invert_config config = { 0 };

	for (unsigned i = 0; i < BL_ACQ__SRC_COUNT; i++) {
		config.invert[i] = main_menu_conifg_get_channel_inverted(i);
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

	filter->ctx = data_invert_init(&config, frequency, channels, src_mask);
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
 * Resister the normalising filter, if enabled.
 *
 * \param[in]  frequency  The sampling frequency.
 * \param[in]  channels   The number of channels.
 * \param[in]  src_mask   Mask of enabled sources.
 * \return true on success, or false on error.
 */
static bool data__register_ac_denoise(
		unsigned frequency,
		unsigned channels,
		uint32_t src_mask)
{
	struct data_filter *filter;
	struct data_avg_config config = {
		.filter_freq = (frequency / main_menu_conifg_get_filter_ac_denoise_frequency()) * 1024,
		.normalise   = false,
	};

	filter = data__allocate_filter();
	if (filter == NULL) {
		return false;
	}

	filter->ctx = data_avg_init(&config, frequency, channels, src_mask);
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
 * Initialise the data context.
 *
 * \param[in]  calibrate  Whether this is a calibration acquisition.
 * \param[in]  frequency  The sampling frequency.
 * \param[in]  channels   The number of channels.
 * \param[in]  src_mask   Mask of enabled sources.
 * \return true on success, or false on error.
 */
static bool data__register_filters(
		bool calibrate,
		unsigned frequency,
		unsigned channels,
		uint32_t src_mask)
{
	BV_UNUSED(calibrate);

	if (!data__register_invert(frequency, channels, src_mask)) {
		return false;
	}

	if (main_menu_conifg_get_filter_normalise_enabled() &&
			!data__register_normalise(
					frequency, channels, src_mask)) {
		return false;
	}

	if (main_menu_conifg_get_filter_ac_denoise_enabled() &&
			!data__register_ac_denoise(
					frequency, channels, src_mask)) {
		return false;
	}

	return true;
}

/* Exported interface, documented in data.h */
bool data_start(bool calibrate, unsigned frequency, unsigned src_mask)
{
	unsigned channels = util_bit_count(src_mask);
	unsigned n = 0;

	assert(data_g.enabled == false);

	for (unsigned i = 0; i < BV_ARRAY_LEN(data_g.mapping); i++) {
		if (src_mask & (1u << i)) {
			data_g.mapping[i] = n++;
		} else {
			data_g.mapping[i] = UINT_MAX;
		}
	}

	if (!graph_init()) {
		return false;
	}

	if (!data__register_filters(calibrate, frequency, channels, src_mask)) {
		data_finish();
		return false;
	}

	for (unsigned i = 0; i < channels; i++) {
		if (!graph_create(i, frequency)) {
			data_finish();
			return false;
		}
	}

	data_g.enabled = true;

	return true;
}
