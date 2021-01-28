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
 * \brief Implementation of the data processing pipeline.
 */

#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

#include <cyaml/cyaml.h>

#include "common/acq.h"

#include "../util.h"

#include "dpp.h"
#include "param.h"

/** CYAML schema: Value types. */
static const cyaml_strval_t bv_value_kind[] = {
	{ .val = BV_VALUE_BOOL,     .str = "bool" },
	{ .val = BV_VALUE_DOUBLE,   .str = "double" },
	{ .val = BV_VALUE_UNSIGNED, .str = "unsigned" },
};

/** CYAML schema: Value union fields. */
static const cyaml_schema_field_t bv_value_fields[] = {
	CYAML_FIELD_ENUM("kind", CYAML_FLAG_OPTIONAL,
			struct bv_value, type,
			bv_value_kind, CYAML_ARRAY_LEN(bv_value_kind)),
	CYAML_FIELD_BOOL("bool", CYAML_FLAG_DEFAULT,
			struct bv_value, type_bool),
	CYAML_FIELD_FLOAT("double", CYAML_FLAG_DEFAULT,
			struct bv_value, type_double),
	CYAML_FIELD_UINT("unsigned", CYAML_FLAG_DEFAULT,
			struct bv_value, type_unsigned),
	CYAML_FIELD_END
};

/** CYAML schema: Pipeline stage node types. */
static const cyaml_strval_t bv_node_kind[] = {
	{ .val = BV_NODE_GRAPH,   .str = "graph" },
	{ .val = BV_NODE_FILTER,  .str = "filter" },
	{ .val = BV_NODE_CHANNEL, .str = "channel" },
};

/** CYAML schema: Node graph fields. */
static const cyaml_schema_field_t bv_node_graph_fields[] = {
	CYAML_FIELD_STRING_PTR("label", CYAML_FLAG_POINTER,
			struct bv_node_graph, label,
			0, CYAML_UNLIMITED),
	CYAML_FIELD_END
};

/** CYAML schema: Node filter fields. */
static const cyaml_schema_field_t bv_node_filter_fields[] = {
	CYAML_FIELD_STRING_PTR("label", CYAML_FLAG_POINTER,
			struct bv_node_filter, label,
			0, CYAML_UNLIMITED),
	CYAML_FIELD_STRING_PTR("endpoint", CYAML_FLAG_POINTER,
			struct bv_node_filter, endpoint,
			0, CYAML_UNLIMITED),
	CYAML_FIELD_END
};

/** CYAML schema: Node channel fields. */
static const cyaml_schema_field_t bv_node_channel_fields[] = {
	CYAML_FIELD_STRING_PTR("label", CYAML_FLAG_POINTER,
			struct bv_node_channel, label,
			0, CYAML_UNLIMITED),
	CYAML_FIELD_END
};

/** CYAML schema: Node union fields. */
static const cyaml_schema_field_t bv_node_fields[] = {
	CYAML_FIELD_ENUM("kind", CYAML_FLAG_OPTIONAL,
			struct bv_node, type,
			bv_node_kind, CYAML_ARRAY_LEN(bv_node_kind)),
	CYAML_FIELD_MAPPING("graph", CYAML_FLAG_DEFAULT,
			struct bv_node, graph,
			bv_node_graph_fields),
	CYAML_FIELD_MAPPING("filter", CYAML_FLAG_DEFAULT,
			struct bv_node, filter,
			bv_node_filter_fields),
	CYAML_FIELD_MAPPING("channel", CYAML_FLAG_DEFAULT,
			struct bv_node, channel,
			bv_node_channel_fields),
	CYAML_FIELD_END
};

/** CYAML schema: Endpoint kinds. */
static const cyaml_strval_t bv_endpoint_kind[] = {
	{ .val = BV_ENDPOINT_STREAM, .str = "stream" },
	{ .val = BV_ENDPOINT_VALUE,  .str = "value" },
};

/** CYAML schema: Endpoint structure fields. */
static const cyaml_schema_field_t bv_endpoint_fields[] = {
	CYAML_FIELD_STRING_PTR("name", CYAML_FLAG_POINTER,
			struct bv_endpoint, name,
			0, CYAML_UNLIMITED),
	CYAML_FIELD_ENUM("kind", CYAML_FLAG_DEFAULT,
			struct bv_endpoint, kind,
			bv_endpoint_kind, CYAML_ARRAY_LEN(bv_endpoint_kind)),
	CYAML_FIELD_END
};

/** CYAML schema: Endpoint structure. */
static const cyaml_schema_value_t bv_endpoint = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, struct bv_endpoint,
			bv_endpoint_fields),
};

/** CYAML schema: Parameter spec fields. */
static const cyaml_schema_field_t bv_param_spec_fields[] = {
	CYAML_FIELD_STRING_PTR("name", CYAML_FLAG_POINTER,
			struct bv_param_spec, name,
			0, CYAML_UNLIMITED),
	CYAML_FIELD_ENUM("kind", CYAML_FLAG_DEFAULT,
			struct bv_param_spec, kind,
			bv_value_kind, CYAML_ARRAY_LEN(bv_value_kind)),
	CYAML_FIELD_END
};

/** CYAML schema: Parameter spec structure. */
static const cyaml_schema_value_t bv_param_spec = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, struct bv_param_spec,
			bv_param_spec_fields),
};

/** CYAML schema: Parameter structure fields. */
static const cyaml_schema_field_t bv_params_fields[] = {
	CYAML_FIELD_STRING_PTR("name", CYAML_FLAG_POINTER,
			struct bv_param, name,
			0, CYAML_UNLIMITED),
	CYAML_FIELD_UNION("value", CYAML_FLAG_DEFAULT,
			struct bv_param, value,
			bv_value_fields,
			"kind"),
	CYAML_FIELD_END
};

/** CYAML schema: Parameter structure. */
static const cyaml_schema_value_t bv_params = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, struct bv_param,
			bv_params_fields),
};

/** CYAML schema: Filter structure fields. */
static const cyaml_schema_field_t bv_filter_fields[] = {
	CYAML_FIELD_STRING_PTR("name", CYAML_FLAG_POINTER,
			struct bv_filter, name,
			0, CYAML_UNLIMITED),
	CYAML_FIELD_SEQUENCE("parameters",
			CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
			struct bv_filter, param,
			&bv_param_spec, 0, CYAML_UNLIMITED),
	CYAML_FIELD_SEQUENCE("input", CYAML_FLAG_POINTER,
			struct bv_filter, input,
			&bv_endpoint, 0, CYAML_UNLIMITED),
	CYAML_FIELD_SEQUENCE("output", CYAML_FLAG_POINTER,
			struct bv_filter, output,
			&bv_endpoint, 0, CYAML_UNLIMITED),
	CYAML_FIELD_END
};

/** CYAML schema: Filter structure. */
static const cyaml_schema_value_t bv_filter = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, struct bv_filter,
			bv_filter_fields),
};

/** CYAML schema: Valid acquisition emission mode mapping. */
static const cyaml_strval_t bv_acq_emission_mode[] = {
	{ .val = BL_ACQ_CONTINUOUS, .str = "Continuous" },
	{ .val = BL_ACQ_FLASH,      .str = "Flash" },
};

/** CYAML schema: Channel structure fields. */
static const cyaml_schema_field_t bv_channel_fields[] = {
	CYAML_FIELD_STRING_PTR("label", CYAML_FLAG_POINTER,
			struct bv_channel, label,
			0, CYAML_UNLIMITED),
	CYAML_FIELD_UINT("channel", CYAML_FLAG_DEFAULT,
			struct bv_channel, channel),
	CYAML_FIELD_END
};

/** CYAML schema: Channel structure. */
static const cyaml_schema_value_t bv_channel = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, struct bv_channel,
			bv_channel_fields),
};

/** CYAML schema: Pipeline stage node types. */
static const cyaml_strval_t bv_colour_kind[] = {
	{ .val = BV_COLOUR_RGB, .str = "rgb" },
	{ .val = BV_COLOUR_HSV, .str = "hsv" },
};

/** CYAML schema: Colour RGB fields. */
static const cyaml_schema_field_t bv_colour_rgb_fields[] = {
	CYAML_FIELD_UINT("r", CYAML_FLAG_DEFAULT,
			struct bv_colour_rgb, r),
	CYAML_FIELD_UINT("g", CYAML_FLAG_DEFAULT,
			struct bv_colour_rgb, g),
	CYAML_FIELD_UINT("b", CYAML_FLAG_DEFAULT,
			struct bv_colour_rgb, b),
	CYAML_FIELD_END
};

/** CYAML schema: Colour HSV fields. */
static const cyaml_schema_field_t bv_colour_hsv_fields[] = {
	CYAML_FIELD_UINT("h", CYAML_FLAG_DEFAULT,
			struct bv_colour_hsv, h),
	CYAML_FIELD_UINT("s", CYAML_FLAG_DEFAULT,
			struct bv_colour_hsv, s),
	CYAML_FIELD_UINT("v", CYAML_FLAG_DEFAULT,
			struct bv_colour_hsv, v),
	CYAML_FIELD_END
};

/** CYAML schema: Colour fields. */
static const cyaml_schema_field_t bv_colour_fields[] = {
	CYAML_FIELD_ENUM("kind", CYAML_FLAG_OPTIONAL,
			struct bv_colour, type,
			bv_colour_kind, CYAML_ARRAY_LEN(bv_colour_kind)),
	CYAML_FIELD_MAPPING("rgb", CYAML_FLAG_DEFAULT,
			struct bv_colour, rgb,
			bv_colour_rgb_fields),
	CYAML_FIELD_MAPPING("hsv", CYAML_FLAG_DEFAULT,
			struct bv_colour, hsv,
			bv_colour_hsv_fields),
	CYAML_FIELD_END
};

/** CYAML schema: Graph structure fields. */
static const cyaml_schema_field_t bv_graph_fields[] = {
	CYAML_FIELD_STRING_PTR("label", CYAML_FLAG_POINTER,
			struct bv_graph, label,
			0, CYAML_UNLIMITED),
	CYAML_FIELD_STRING_PTR("name", CYAML_FLAG_POINTER,
			struct bv_graph, name,
			0, CYAML_UNLIMITED),
	CYAML_FIELD_UNION("colour", CYAML_FLAG_DEFAULT,
			struct bv_graph, colour,
			bv_colour_fields,
			"kind"),
	CYAML_FIELD_END
};

/** CYAML schema: Graph structure. */
static const cyaml_schema_value_t bv_graph = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, struct bv_graph,
			bv_graph_fields),
};

/** CYAML schema: Processing structure fields. */
static const cyaml_schema_field_t bv_context_fields[] = {
	CYAML_FIELD_STRING_PTR("pipeline", CYAML_FLAG_POINTER,
			struct bv_context, pipeline,
			0, CYAML_UNLIMITED),
	CYAML_FIELD_SEQUENCE("channels", CYAML_FLAG_POINTER,
			struct bv_context, channel,
			&bv_channel, 0, CYAML_UNLIMITED),
	CYAML_FIELD_SEQUENCE("graphs", CYAML_FLAG_POINTER,
			struct bv_context, graph,
			&bv_graph, 0, CYAML_UNLIMITED),
	CYAML_FIELD_END
};

/** CYAML schema: Context structure. */
static const cyaml_schema_value_t bv_context = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, struct bv_context,
			bv_context_fields),
};

/** CYAML schema: Setup structure fields. */
static const cyaml_schema_field_t bv_setup_fields[] = {
	CYAML_FIELD_STRING_PTR("name", CYAML_FLAG_POINTER,
			struct bv_setup, name,
			0, CYAML_UNLIMITED),
	CYAML_FIELD_ENUM("mode", CYAML_FLAG_DEFAULT,
			struct bv_setup, acq_mode,
			bv_acq_emission_mode,
			CYAML_ARRAY_LEN(bv_acq_emission_mode)),
	CYAML_FIELD_SEQUENCE("contexts", CYAML_FLAG_POINTER,
			struct bv_setup, context,
			&bv_context, 0, CYAML_UNLIMITED),
	CYAML_FIELD_END
};

/** CYAML schema: Setup structure. */
static const cyaml_schema_value_t bv_setup = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, struct bv_setup,
			bv_setup_fields),
};

/** CYAML schema: Pipeline filter fields. */
static const cyaml_schema_field_t bv_pipeline_filter_fields[] = {
	CYAML_FIELD_STRING_PTR("label", CYAML_FLAG_POINTER,
			struct bv_pipeline_filter, label,
			0, CYAML_UNLIMITED),
	CYAML_FIELD_STRING_PTR("filter", CYAML_FLAG_POINTER,
			struct bv_pipeline_filter, filter,
			0, CYAML_UNLIMITED),
	CYAML_FIELD_SEQUENCE("parameters",
			CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
			struct bv_pipeline_filter, parameters,
			&bv_params, 0, CYAML_UNLIMITED),
	CYAML_FIELD_END
};

/** CYAML schema: Pipeline filter details. */
static const cyaml_schema_value_t bv_pipeline_filter = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, struct bv_pipeline_filter,
			bv_pipeline_filter_fields),
};

/** CYAML schema: Pipeline stage fields. */
static const cyaml_schema_field_t bv_pipeline_stage_fields[] = {
	CYAML_FIELD_UNION("from", CYAML_FLAG_DEFAULT,
			struct bv_pipeline_stage, from,
			bv_node_fields,
			"kind"),
	CYAML_FIELD_UNION("to", CYAML_FLAG_DEFAULT,
			struct bv_pipeline_stage, to,
			bv_node_fields,
			"kind"),
	CYAML_FIELD_END
};

/** CYAML schema: Pipeline stage. */
static const cyaml_schema_value_t bv_pipeline_stage = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, struct bv_pipeline_stage,
			bv_pipeline_stage_fields),
};

/** CYAML schema: Pipeline structure fields. */
static const cyaml_schema_field_t bv_pipeline_fields[] = {
	CYAML_FIELD_STRING_PTR("name", CYAML_FLAG_POINTER,
			struct bv_pipeline, name,
			0, CYAML_UNLIMITED),
	CYAML_FIELD_SEQUENCE("filters",
			CYAML_FLAG_OPTIONAL | CYAML_FLAG_POINTER,
			struct bv_pipeline, filter,
			&bv_pipeline_filter, 0, CYAML_UNLIMITED),
	CYAML_FIELD_SEQUENCE("stages", CYAML_FLAG_POINTER,
			struct bv_pipeline, stage,
			&bv_pipeline_stage, 0, CYAML_UNLIMITED),
	CYAML_FIELD_END
};

/** CYAML schema: Pipeline mapping. */
static const cyaml_schema_value_t bv_pipeline = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, struct bv_pipeline,
			bv_pipeline_fields),
};

/** CYAML schema: Top level fields. */
static const cyaml_schema_field_t bv_top_fields[] = {
	CYAML_FIELD_SEQUENCE("filters",
			CYAML_FLAG_OPTIONAL | CYAML_FLAG_POINTER,
			struct dpp, filters,
			&bv_filter, 0, CYAML_UNLIMITED),
	CYAML_FIELD_SEQUENCE("pipelines",
			CYAML_FLAG_OPTIONAL | CYAML_FLAG_POINTER,
			struct dpp, pipeline,
			&bv_pipeline, 0, CYAML_UNLIMITED),
	CYAML_FIELD_SEQUENCE("setup",
			CYAML_FLAG_OPTIONAL | CYAML_FLAG_POINTER,
			struct dpp, setup,
			&bv_setup, 0, CYAML_UNLIMITED),
	CYAML_FIELD_END
};

/** CYAML schema: Top level mapping. */
static const cyaml_schema_value_t bv_top = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_POINTER, struct dpp, bv_top_fields),
};

/** CYAML config data. */
static const cyaml_config_t config = {
	.log_level = CYAML_LOG_WARNING, /* Logging errors and warnings only. */
	.log_fn    = cyaml_log,         /* Use the default logging function. */
	.mem_fn    = cyaml_mem,         /* Use the default memory allocator. */
};

struct dpp *dpp_file_load(
		const char *resources_dir_path)
{
	struct dpp *dpp = NULL;
	cyaml_err_t err;
	char *path;

	path = util_create_path(resources_dir_path, "filters.yaml");
	if (path == NULL) {
		goto error;
	}

	err = cyaml_load_file(path, &config, &bv_top, (void **) &dpp, 0);
	free(path);
	if (err != CYAML_OK) {
		fprintf(stderr, "ERROR: %s\n", cyaml_strerror(err));
		goto error;
	}

	return dpp;

error:
	if (dpp != NULL) {
		cyaml_free(&config, &bv_top, dpp, 0);
	}

	return NULL;
}

void dpp_file_free(struct dpp *dpp)
{
	cyaml_free(&config, &bv_top, dpp, 0);
}
