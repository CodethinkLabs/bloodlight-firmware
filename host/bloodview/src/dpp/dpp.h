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
 * \brief Interface to the data processing pipeline.
 */

#ifndef BV_DPP_DPP_H
#define BV_DPP_DPP_H

#include "common/acq.h"

#include "value.h"

struct bv_endpoint {
	char *name;
	enum bv_endpoint_kind {
		BV_ENDPOINT_STREAM,
		BV_ENDPOINT_VALUE,
	} kind;
};

struct bv_filter {
	char *name;
	struct bv_param_spec *param;
	struct bv_endpoint *input;
	struct bv_endpoint *output;
	uint32_t param_count;
	uint32_t input_count;
	uint32_t output_count;
};

struct bv_pipeline_filter {
	char *label;
	char *filter;

	struct bv_param *parameters;
	uint32_t parameters_count;
};

struct bv_node {
	enum bv_node_e {
		BV_NODE_GRAPH,
		BV_NODE_FILTER,
		BV_NODE_CHANNEL,
	} type;
	union {
		struct bv_node_graph {
			char *label;
		} graph;
		struct bv_node_filter {
			char *label;
			char *endpoint;
		} filter;
		struct bv_node_channel {
			char *label;
		} channel;
	};
};

struct bv_pipeline_stage {
	struct bv_node from;
	struct bv_node to;
};

struct bv_pipeline {
	char *name;

	struct bv_pipeline_filter *filter;
	uint32_t filter_count;

	struct bv_pipeline_stage *stage;
	uint32_t stage_count;
};

struct bv_channel {
	char *label;
	unsigned channel;
};

/** Bloodview colour. */
struct bv_colour {
	enum bv_colour_e {
		BV_COLOUR_RGB, /**< Colour type: RGB. */
		BV_COLOUR_HSV, /**< Colour type: HSV. */
	} type; /**< Colour type. */
	union {
		struct bv_colour_rgb {
			uint8_t r; /**< Red. */
			uint8_t g; /**< Green. */
			uint8_t b; /**< Blue. */
		} rgb; /**< RGB colour type specification. */
		struct bv_colour_hsv {
			uint16_t h; /**< Hue. Range 0-360. */
			uint8_t  s; /**< Saturation. Range: 0-100. */
			uint8_t  v; /**< Value.  Range: 0-100. */
		} hsv; /**< HSV colour type specification. */
	};
};

struct bv_graph {
	char *label;
	char *name;
	struct bv_colour colour;
};

struct bv_context {
	char *pipeline;

	struct bv_channel *channel;
	uint32_t channel_count;

	struct bv_graph *graph;
	uint32_t graph_count;
};

struct bv_setup {
	char *name;
	enum bl_acq_flash_mode acq_mode;

	struct bv_context *context;
	uint32_t context_count;
};

struct dpp {
	struct bv_filter *filters;
	uint32_t filters_count;

	struct bv_pipeline *pipeline;
	uint32_t pipeline_count;

	struct bv_setup *setup;
	uint32_t setup_count;
};

/**
 * Initialise the data processing pipeline module.
 *
 * Loads the data processing pipeline definitions from disc.
 *
 * \param[in]  resources_dir_path  Path to the resources directory.
 * \return true on success, false otherwise.
 */
bool dpp_init(const char *resources_dir_path);

/**
 * Finalise the data processing pipeline module.
 */
void dpp_fini(void);

/**
 * Get a list of available data processing pipeline setups.
 *
 * \param[out]  list   Returns data processing pipeline setups list on success.
 * \param[out]  count  Returns number of entries in setups list on success.
 * \return true on success, false otherwise.
 */
bool dpp_get_dpp_list(
		char ***list,
		unsigned *count);

/**
 * Start an acquisition using the data processing pipeline setup of given index.
 *
 * The caller owns the returned pipeline array.  It will be freed by
 * \ref dpp_stop. The client must insert channels_out samples into the
 * the first channels out entries of the pipeline prior to calling
 * \ref dpp_process each time.
 *
 * \param[in]  frequency     The sampling rate for the acquisition.
 * \param[in]  dpp_index     The data processing pipeline setup to use.
 * \param[out] pipeline_out  Returns pipeline to insert channel samples into.
 * \param[out] channels_out  Returns number of pipeline slots to be filled.
 * \return true on success, false otherwise.
 */
bool dpp_start(
		unsigned frequency,
		unsigned dpp_index,
		struct bv_value **pipeline_out,
		unsigned *channels_out);

/**
 * Stop an acquisition and clean it up.
 *
 * \param[in]  pipeline  The data processing pipeline.
 * \return true on success, false otherwise.
 */
void dpp_stop(struct bv_value *pipeline);

/**
 * Run the pipeline over the data.
 *
 * The input channel data must have already been inserted into the first
 * entries of the array.
 *
 * \param[in]  pipeline  The data processing pipeline.
 * \return true on success, false otherwise.
 */
bool dpp_process(struct bv_value *pipeline);

/**
 * Get the pipeline's emission mode mode.
 *
 * This is continuous or flash.
 *
 * \param[in]  dpp_index     The data processing pipeline setup to interrogate.
 * \return the emission mode.
 */
enum bl_acq_flash_mode dpp_get_emission_mode(unsigned dpp_index);

/**
 * Get which acquisition channels a pipeline uses.
 *
 * Depending on whether the pipeline is in continuous or flash emission mode,
 * these channels correspond to sources (e.g. photodiodes) or channels (LEDs).
 *
 * \param[in]  dpp_index     The data processing pipeline setup to interrogate.
 * \return the channel mask that the pipeline uses.
 */
unsigned dpp_get_channel_mask(unsigned dpp_index);

#endif
