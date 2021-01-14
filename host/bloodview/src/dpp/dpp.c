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
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>

#include "../util.h"
#include "../graph.h"

#include "dpp.h"
#include "file.h"
#include "filter.h"

#include "filter/average.h"

/** Internal representation of channels. */
struct dpp_channel {
	unsigned channel;    /**< Acquisition channel index. */
	unsigned dpp_offset; /**< Offset in the data processing pipeline. */
};

/** Internal filter input/output representation. */
struct dpp_filter_endpoint {
	bool set; /**< Whether the endpoint has been set internally. */
	unsigned dpp_offset; /**< Offset in the data processing pipeline. */
	const char *name; /**< Endpoint name. */
};

/** Internal filter representation. */
struct dpp_filter {
	const struct bv_context *context; /**< Loaded context context. */
	const struct bv_pipeline_filter *filter; /**< Loaded filter data. */

	struct dpp_filter_endpoint *input; /**< Internal input tracking. */
	unsigned input_count; /**< Input count. */

	struct dpp_filter_endpoint *output; /**< Internal output tracking. */
	unsigned output_count; /**< Output count. */

	unsigned *filter_inputs;
	unsigned *filter_outputs;
};

/** Internal graph representation. */
struct dpp_graph {
	const struct bv_context *context; /**< Loaded context context. */
	const struct bv_graph *graph;     /**< Loaded graph context. */

	unsigned dpp_offset; /**< Offset in the data processing pipeline. */
};

/** Global context for the data processing pipeline module. */
struct {
	struct dpp *dpp; /**< The DPP data loaded from file. */

	unsigned dpp_offset_next; /**< Next free data pipeline offset. */
	unsigned pipeline_len; /**< Length of the data processing pipeline. */
	unsigned frequency; /**< Acquisition frequency. */

	struct dpp_channel *channel; /**< Internal channel tracking. */
	unsigned channel_count;      /**< Number of channels. */

	struct dpp_filter *filter;
	unsigned filter_count;

	struct dpp_graph *graph;
	unsigned graph_count;
} dpp_g; /**< Module's global context. */

/**
 * Clean up internal representation and state.
 *
 * This does not clean up anything loaded from the YAML file.
 */
static void dpp__cleanup(void)
{
	if (dpp_g.channel != NULL) {
		free(dpp_g.channel);
		dpp_g.channel = NULL;
	}
	dpp_g.channel_count = 0;

	if (dpp_g.filter != NULL) {
		for (unsigned i = 0; i < dpp_g.filter_count; i++) {
			free(dpp_g.filter[i].input);
			free(dpp_g.filter[i].output);

			free(dpp_g.filter[i].filter_inputs);
			free(dpp_g.filter[i].filter_outputs);
		}
		free(dpp_g.filter);
		dpp_g.filter = NULL;
	}
	dpp_g.filter_count = 0;

	if (dpp_g.graph != NULL) {
		free(dpp_g.graph);
		dpp_g.graph = NULL;
	}
	dpp_g.graph_count = 0;

	dpp_g.dpp_offset_next = 0;
	dpp_g.pipeline_len = 0;
	dpp_g.frequency = 0;

	filter_finish();
}

/* Exported interface, documented in dpp.h */
void dpp_fini(void)
{
	if (dpp_g.dpp != NULL) {
		dpp_file_free(dpp_g.dpp);
		dpp_g.dpp = NULL;
	}

	dpp__cleanup();
	filter_fini();
}

/**
 * Register the filters we want to be available to the data processing pipeline.
 *
 * \return true on success, false otherwise.
 */
static bool dpp__init_filter(void)
{
	if (!filter_init()) {
		return false;
	}

	if (!filter_average_register()) {
		return false;
	}

	return true;
}

/* Exported interface, documented in dpp.h */
bool dpp_init(
		const char *resources_dir_path)
{
	struct dpp *dpp;

	if (!dpp__init_filter()) {
		goto error;
	}

	dpp = dpp_file_load(resources_dir_path);
	if (dpp == NULL) {
		goto error;
	}

	dpp_g.dpp = dpp;
	return true;

error:
	dpp_fini();
	return false;
}

/**
 * Get pipeline by name.
 *
 * \param[in]  name  Name of pipeline to get.
 * \return The requested pipeline or NULL.
 */
static const struct bv_pipeline *dpp__get_pipeline(
		const char *name)
{
	for (unsigned i = 0; i < dpp_g.dpp->pipeline_count; i++) {
		if (strcmp(name, dpp_g.dpp->pipeline[i].name) == 0) {
			return &dpp_g.dpp->pipeline[i];
		}
	}

	fprintf(stderr, "Error: DPP: No pipeline found with name %s.\n", name);
	return NULL;
}

/**
 * Get pipeline filter from pipeline, by label.
 *
 * \param[in]  pipeline  Pipeline to get filter from.
 * \param[in]  label     Name of pipeline filter to get.
 * \return The requested pipeline filter or NULL.
 */
static const struct bv_pipeline_filter *dpp__get_dpp_pipeline_filter(
		const struct bv_pipeline *pipeline,
		const char *label)
{
	for (unsigned i = 0; i < pipeline->filter_count; i++) {
		if (strcmp(pipeline->filter[i].label, label) == 0) {
			return &pipeline->filter[i];
		}
	}

	fprintf(stderr, "Error: DPP: No filter found with label %s.\n", label);
	return NULL;
}

/**
 * Get filter spec by name.
 *
 * \param[in]  filter_name  Name of filter spec to get.
 * \return The requested filter spec or NULL.
 */
static const struct bv_filter *dpp__get_filter_spec(
		const char *filter_name)
{
	for (unsigned i = 0; i < dpp_g.dpp->filters_count; i++) {
		if (strcmp(dpp_g.dpp->filters[i].name, filter_name) == 0) {
			return &dpp_g.dpp->filters[i];
		}
	}

	fprintf(stderr, "Error: DPP: No filter spec found for %s\n",
			filter_name);
	return NULL;
}

/**
 * Create array for a filter's inputs.
 *
 * \param[in]  filter_name  Filter name to build array for.
 * \param[out] input_count  Returns input count on success.
 * \param[out] input        Returns input array on success.
 * \return true on success, false otherwise.
 */
static bool dpp__get_dpp_filter_inputs(
		const char *filter_name,
		unsigned *input_count,
		struct dpp_filter_endpoint **input)
{
	struct dpp_filter_endpoint *endpoints = NULL;
	const struct bv_filter *filter;
	unsigned count;

	filter = dpp__get_filter_spec(filter_name);
	if (filter == NULL) {
		return false;
	}

	count = filter->input_count;
	if (count > 0) {
		endpoints = calloc(count, sizeof(*endpoints));
		if (endpoints == NULL) {
			return false;
		}
	}

	*input_count = count;
	*input = endpoints;
	return true;
}

/**
 * Get input index for given filter's given input.
 *
 * \param[in]  filter_name  Name of filter to get index for.
 * \param[in]  input_name   Name of input to get index for,
 * \param[out] index_out    Returns input index for filter on success.
 * \return true on success, false otherwise.
 */
static bool dpp__get_input_index(
		const char *filter_name,
		const char *input_name,
		unsigned *index_out)
{
	const struct bv_filter *filter;

	filter = dpp__get_filter_spec(filter_name);
	if (filter == NULL) {
		return false;
	}

	for (unsigned i = 0; i < filter->input_count; i++) {
		if (strcmp(filter->input[i].name, input_name) == 0) {
			*index_out = i;
			return true;
		}
	}

	fprintf(stderr, "Error: DPP: Filter %s has no input %s\n",
			filter_name, input_name);
	return false;
}

/**
 * Create array for a filter's outputs.
 *
 * \param[in]  filter_name   Filter name to build array for.
 * \param[out] output_count  Returns output count on success.
 * \param[out] output        Returns output array on success.
 * \return true on success, false otherwise.
 */
static bool dpp__get_dpp_filter_outputs(
		const char *filter_name,
		unsigned *output_count,
		struct dpp_filter_endpoint **output)
{
	struct dpp_filter_endpoint *endpoints = NULL;
	const struct bv_filter *filter;
	unsigned count;

	filter = dpp__get_filter_spec(filter_name);
	if (filter == NULL) {
		return false;
	}

	count = filter->output_count;
	if (count > 0) {
		endpoints = calloc(count, sizeof(*endpoints));
		if (endpoints == NULL) {
			return false;
		}
	}

	*output_count = count;
	*output = endpoints;
	return true;
}

/**
 * Get output index for given filter's given output.
 *
 * \param[in]  filter_name  Name of filter to get index for.
 * \param[in]  output_name  Name of output to get index for,
 * \param[out] index_out    Returns output index for filter on success.
 * \return true on success, false otherwise.
 */
static bool dpp__get_output_index(
		const char *filter_name,
		const char *output_name,
		unsigned *index_out)
{
	const struct bv_filter *filter;

	filter = dpp__get_filter_spec(filter_name);
	if (filter == NULL) {
		return false;
	}

	for (unsigned i = 0; i < filter->output_count; i++) {
		if (strcmp(filter->output[i].name, output_name) == 0) {
			*index_out = i;
			return true;
		}
	}

	fprintf(stderr, "Error: DPP: Filter %s has no output %s\n",
			filter_name, output_name);
	return false;
}

/**
 * Get internal representation for a filter.
 *
 * If the filter currently has no internal representation, one is created,
 * otherwise the existing one is returned.
 *
 * \param[in]  context  Context in which the filter is instantiated.
 * \param[in]  filter   Filter to get internal representation for.
 * \param[out] out      Returns the internal filter representation on success.
 * \return true on success, false otherwise.
 */
static bool dpp__get_dpp_filter(
		const struct bv_context *context,
		const struct bv_pipeline_filter *filter,
		struct dpp_filter **out)
{
	struct dpp_filter_endpoint *output;
	struct dpp_filter_endpoint *input;
	struct dpp_filter *f;
	unsigned out_count;
	unsigned in_count;
	unsigned count;

	for (unsigned i = 0; i < dpp_g.filter_count; i++) {
		if (dpp_g.filter[i].context == context &&
		    dpp_g.filter[i].filter  == filter) {
			*out = &dpp_g.filter[i];
			return true;
		}
	}

	if (!dpp__get_dpp_filter_inputs(filter->filter,
			&in_count, &input)) {
		return false;
	}

	if (!dpp__get_dpp_filter_outputs(filter->filter,
			&out_count, &output)) {
		free(input);
		return false;
	}

	count = dpp_g.filter_count + 1;
	f = realloc(dpp_g.filter, sizeof(*f) * count);
	if (f == NULL) {
		free(input);
		free(output);
		return false;
	}
	memset(f +  dpp_g.filter_count, 0, sizeof(*f));

	f[dpp_g.filter_count].context = context;
	f[dpp_g.filter_count].filter = filter;

	f[dpp_g.filter_count].output_count = out_count;
	f[dpp_g.filter_count].output = output;

	f[dpp_g.filter_count].input_count = in_count;
	f[dpp_g.filter_count].input = input;

	*out = &f[dpp_g.filter_count];
	dpp_g.filter = f;
	dpp_g.filter_count++;

	return true;
}

/**
 * Get data processing pipeline slot for a channel.
 *
 * If the channel currently has no slot assigned, a new one is assigned,
 * otherwise the existing one is returned.
 *
 * \param[in]  channel  Channel to get slot for.
 * \param[out] offset   Returns the slot on success.
 * \return true on success, false otherwise.
 */
static bool dpp__get_slot_for_channel(
		unsigned channel,
		unsigned *offset)
{
	struct dpp_channel *c;
	unsigned count;

	for (unsigned i = 0; i < dpp_g.channel_count; i++) {
		if (dpp_g.channel[i].channel == channel) {
			*offset = dpp_g.channel[i].dpp_offset;
			return true;
		}
	}

	count = dpp_g.channel_count + 1;
	c = realloc(dpp_g.channel, count * sizeof(*c));
	if (c == NULL) {
		fprintf(stderr, "Error: realloc fail.\n");
		return false;
	}
	dpp_g.channel = c;

	c[dpp_g.channel_count].channel = channel;
	c[dpp_g.channel_count].dpp_offset = dpp_g.dpp_offset_next;
	dpp_g.channel_count++;

	*offset = dpp_g.dpp_offset_next;
	dpp_g.dpp_offset_next++;
	return true;
}

/**
 * Get acquisition channel for a node of channel type.
 *
 * \param[in]  ctx      Context that node exists in.
 * \param[in]  c        Node of channel type.
 * \param[out] channel  Returns the aquisition channel for the node on success.
 * \return true on success, false otherwise.
 */
static bool dpp__get_channel_for_pipeline_node(
		const struct bv_context *ctx,
		const struct bv_node_channel *c,
		unsigned *channel)
{
	assert(ctx != NULL);
	assert(c != NULL);

	for (unsigned i = 0; i < ctx->channel_count; i++) {
		if (strcmp(c->label, ctx->channel[i].label) == 0) {
			*channel = ctx->channel[i].channel;
			return true;
		}
	}

	fprintf(stderr, "Error: DPP: No channel found for label: %s.\n",
			c->label);
	return false;
}

/**
 * Get a node's label.
 *
 * \param[in]  n  Node to get label for.
 * \return The node label, or NULL.
 */
static inline const char *dpp__get_node_label(
		const struct bv_node *n)
{
	const char *ret = NULL;

	switch (n->type) {
	case BV_NODE_GRAPH:   ret = n->graph.label;   break;
	case BV_NODE_FILTER:  ret = n->filter.label;  break;
	case BV_NODE_CHANNEL: ret = n->channel.label; break;
	}

	if (ret == NULL) {
		fprintf(stderr, "Error: DPP: Couldn't get node label.\n");
	}

	return ret;
}

/**
 * Get the next free dpp_offset.
 *
 * After an offset is returned, the same offset will not be returned again
 * until the dpp module is cleaned up.
 *
 * \return Next free dpp_offset value.
 */
static inline unsigned dpp__next_dpp_offset(void)
{
	return dpp_g.dpp_offset_next++;
}

/**
 * Get dpp_offset slot for a node.
 *
 * \param[in]  ctx     Context that node exists in.
 * \param[in]  node    The node to get slot for.
 * \param[out] offset  Returns the slot for the node on success.
 * \return true on success, false otherwise.
 */
static bool dpp__pipeline_get_node_slot(
		const struct bv_context *ctx,
		const struct bv_node *node,
		unsigned *offset)
{
	const char *label;

	assert(ctx != NULL);
	assert(node != NULL);
	assert(offset != NULL);

	label = dpp__get_node_label(node);

	switch (node->type) {
	case BV_NODE_CHANNEL:
		if (label != NULL) {
			unsigned channel;
			if (!dpp__get_channel_for_pipeline_node(ctx,
					&node->channel, &channel)) {
				return false;
			}
			return dpp__get_slot_for_channel(channel, offset);
		}
		break;

	case BV_NODE_FILTER:
		if (label != NULL) {
			const struct bv_pipeline *p;
			const struct bv_pipeline_filter *pipeline_filter;
			struct dpp_filter *filter;
			unsigned index;

			p = dpp__get_pipeline(ctx->pipeline);
			if (p == NULL) {
				return false;
			}

			pipeline_filter = dpp__get_dpp_pipeline_filter(p,
					label);
			if (pipeline_filter == NULL) {
				return false;
			}

			if (!dpp__get_dpp_filter(ctx, pipeline_filter,
					&filter)) {
				return false;
			}

			if (!dpp__get_output_index(pipeline_filter->filter,
					node->filter.endpoint, &index)) {
				return false;
			}

			assert(index < filter->output_count);

			if (filter->output[index].set == false) {
				filter->output[index].dpp_offset =
						dpp__next_dpp_offset();
				filter->output[index].set = true;
			}

			*offset = filter->output[index].dpp_offset;
			return true;
		}
		break;

	case BV_NODE_GRAPH:
		fprintf(stderr, "Error: %s called with Graph node\n", __func__);
		break;
	}

	return false;
}

/**
 * Scan the given setup for channels.
 *
 * This sets up the internal representation for channels.
 *
 * \param[in]  s  The setup to scan.
 * \return true on success, false otherwise.
 */
static bool dpp__channel_scan(
		const struct bv_setup *s)
{
	for (unsigned i = 0; i < s->context_count; i++) {
		const struct bv_context *ctx = &s->context[i];
		for (unsigned j = 0; j < ctx->channel_count; j++) {
			unsigned slot;
			if (!dpp__get_slot_for_channel(
					ctx->channel[j].channel, &slot)) {
				return false;
			}
		}
	}

	return true;
}

/**
 * Actualise a filter's endpoint in the internal representation.
 *
 * Creates the endpoint for given `node`.  If it's a From node, `node` and
 * `from` will be the same, otherwise it's a To node.  To nodes signify inputs,
 * From nodes signify outputs.
 *
 * \param[in]  ctx        Context in which the filter is instantiated.
 * \param[in]  p          Pipeline containing the filter.
 * \param[in]  from       The From node of the current pipeline stage.
 * \param[in]  node       Node in current pipeline stage to make endpoint for.
 * \return true on success, false otherwise.
 */
static bool dpp__imbue_filter(
		const struct bv_context *ctx,
		const struct bv_pipeline *p,
		const struct bv_node *from,
		const struct bv_node *node)
{
	const char *label;
	const struct bv_pipeline_filter *pipeline_filter;
	struct dpp_filter *filter;
	unsigned index;

	assert(from != NULL);
	assert(node != NULL);
	assert(ctx != NULL);
	assert(p != NULL);

	if (node->type != BV_NODE_FILTER) {
		return true;
	}

	label = dpp__get_node_label(node);
	if (label == NULL) {
		return false;
	}

	pipeline_filter = dpp__get_dpp_pipeline_filter(p, label);
	if (pipeline_filter == NULL) {
		return false;
	}

	if (!dpp__get_dpp_filter(ctx, pipeline_filter, &filter)) {
		return false;
	}

	if (node != from) {
		/* To node; an input. */
		unsigned dpp_offset;

		if (!dpp__get_input_index(pipeline_filter->filter,
				node->filter.endpoint, &index)) {
			return false;
		}

		assert(index < filter->input_count);
		assert(filter->input[index].set == false);

		if (!dpp__pipeline_get_node_slot(ctx, from, &dpp_offset)) {
			return false;
		}

		filter->input[index].name = node->filter.endpoint;
		filter->input[index].dpp_offset = dpp_offset;
		filter->input[index].set = true;
	} else {
		/* From node; an output. */
		if (!dpp__get_output_index(pipeline_filter->filter,
				node->filter.endpoint, &index)) {
			fprintf(stderr, "%s\n", __func__);
			return false;
		}

		assert(index < filter->output_count);
		assert(filter->output[index].set == false);

		filter->output[index].name = node->filter.endpoint;
		filter->output[index].dpp_offset = dpp__next_dpp_offset();
		filter->output[index].set = true;
	}

	return true;
}

/**
 * Validate the internal representation of filters.
 *
 * \return true on success, false otherwise.
 */
static bool dpp__filter_validate(void)
{
	for (unsigned i = 0; i < dpp_g.filter_count; i++) {
		struct dpp_filter *f = &dpp_g.filter[i];

		for (unsigned j = 0; j < f->input_count; j++) {
			if (f->input[j].set == false) {
				fprintf(stderr, "Error: Filter %s: "
						"input %s unset\n",
						f->filter->filter,
						f->input[j].name);
				return false;
			}
			if (f->output[j].set == false) {
				fprintf(stderr, "Error: Filter %s: "
						"output %s unset\n",
						f->filter->filter,
						f->output[j].name);
				return false;
			}
		}
	}

	return true;
}

/**
 * Create a filter for a particular filter's internal representation.
 *
 * \param[in]  f  Filter internal representation to create filter for.
 * \return true on success, false otherwise.
 */
static bool dpp__filter_create_filter(struct dpp_filter *f)
{
	unsigned *input = malloc(f->input_count * sizeof(*input));
	unsigned *output = malloc(f->output_count * sizeof(*output));

	if (input == NULL || output == NULL) {
		free(input);
		free(output);
		return false;
	}

	for (unsigned i = 0; i < f->input_count; i++) {
		input[i] = f->input[i].dpp_offset;
	}

	for (unsigned i = 0; i < f->output_count; i++) {
		output[i] = f->output[i].dpp_offset;
	}

	f->filter_inputs = input;
	f->filter_outputs = output;

	if (!filter_add(f->filter->filter,
			f->filter->parameters,
			f->filter_outputs,
			f->filter_inputs,
			f->filter->parameters_count,
			f->output_count,
			f->input_count)) {
		return false;
	}

	return true;
}

/**
 * Create filters from the internal representation.
 *
 * \return true on success, false otherwise.
 */
static bool dpp__filter_create(void)
{
	for (unsigned i = 0; i < dpp_g.filter_count; i++) {
		struct dpp_filter *f = &dpp_g.filter[i];

		if (!dpp__filter_create_filter(f)) {
			return false;
		}
	}

	return true;
}

/**
 * Scan the given setup for filters.
 *
 * This sets up the internal representation for filters.
 *
 * \param[in]  s  The setup to scan.
 * \return true on success, false otherwise.
 */
static bool dpp__filter_scan(
		const struct bv_setup *s)
{
	for (unsigned i = 0; i < s->context_count; i++) {
		const struct bv_context *ctx = &s->context[i];
		const struct bv_pipeline *p;

		p = dpp__get_pipeline(ctx->pipeline);
		if (p == NULL) {
			return false;
		}

		for (unsigned j = 0; j < p->stage_count; j++) {
			const struct bv_pipeline_stage *stage = &p->stage[j];

			if (!dpp__imbue_filter(ctx, p,
					&stage->from, &stage->from)) {
				return false;
			}
			if (!dpp__imbue_filter(ctx, p,
					&stage->from, &stage->to)) {
				return false;
			}
		}
	}

	if (!dpp__filter_validate()) {
		return false;
	}

	if (!dpp__filter_create()) {
		return false;
	}

	return true;
}

/**
 * Get a graph from a context by label.
 *
 * \param[in]  ctx        Context in which the graph is instantiated.
 * \param[in]  label      The graph's label.
 * \return The matching graph spec or NULL on failure.
 */
static const struct bv_graph *dpp__get_graph_for_ctx(
		const struct bv_context *ctx,
		const char *label)
{
	for (unsigned i = 0; i < ctx->graph_count; i++) {
		if (strcmp(ctx->graph[i].label, label) == 0) {
			return &ctx->graph[i];
		}
	}

	fprintf(stderr, "Graph %s not found\n", label);
	return NULL;
}

/**
 * Get internal representation for a graph.
 *
 * If the graph currently has no internal representation, one is created,
 * otherwise the existing one is returned.
 *
 * \param[in]  ctx        Context in which the graph is instantiated.
 * \param[in]  graph      Graph to get internal representation for.
 * \param[out] dpp_graph  Returns the internal graph representation on success.
 * \return true on success, false otherwise.
 */
static bool dpp__get_dpp_graph(
		const struct bv_context *ctx,
		const struct bv_graph *graph,
		struct dpp_graph **dpp_graph)
{
	struct dpp_graph *g;
	unsigned count;

	for (unsigned i = 0; i < dpp_g.graph_count; i++) {
		if (dpp_g.graph[i].context == ctx &&
		    dpp_g.graph[i].graph == graph) {
			*dpp_graph = &dpp_g.graph[i];
			return true;
		}
	}

	count = dpp_g.graph_count + 1;
	g = realloc(dpp_g.graph, count * sizeof(*g));
	if (g == NULL) {
		fprintf(stderr, "Error: realloc fail.\n");
		return false;
	}
	dpp_g.graph = g;

	g[dpp_g.graph_count].context = ctx;
	g[dpp_g.graph_count].graph = graph;

	*dpp_graph = &g[dpp_g.graph_count];
	dpp_g.graph_count++;
	return true;
}

/**
 * Build internal representation for a graph node.
 *
 * \param[in]  ctx    Context in which the graph is instantiated.
 * \param[in]  stage  Pipeline stage with graph in To endpoint.
 * \return true on success, false otherwise.
 */
static bool dpp__add_graph(
		const struct bv_context *ctx,
		const struct bv_pipeline_stage *stage)
{
	const struct bv_graph *graph;
	struct dpp_graph *dpp_graph;

	assert(stage->from.type != BV_NODE_GRAPH);
	assert(stage->to.type == BV_NODE_GRAPH);

	graph = dpp__get_graph_for_ctx(ctx, stage->to.graph.label);
	if (graph == NULL) {
		return false;
	}

	if (!dpp__get_dpp_graph(ctx, graph, &dpp_graph)) {
		return false;
	}

	if (!dpp__pipeline_get_node_slot(ctx, &stage->from,
			&dpp_graph->dpp_offset)) {
		return false;
	}

	/* TODO: How to choose graph colour? */
	if (!graph_create(dpp_g.graph_count - 1, dpp_g.frequency,
			graph->name, (SDL_Color){ .r = 255, .b = 255, })) {

	}

	return true;
}

/**
 * Scan the given setup for graphs.
 *
 * This sets up the internal representation for graphs.
 *
 * \param[in]  s  The setup to scan.
 * \return true on success, false otherwise.
 */
static bool dpp__graph_scan(
		const struct bv_setup *s)
{
	for (unsigned i = 0; i < s->context_count; i++) {
		const struct bv_context *ctx = &s->context[i];
		const struct bv_pipeline *p;

		p = dpp__get_pipeline(ctx->pipeline);
		if (p == NULL) {
			return false;
		}

		for (unsigned j = 0; j < p->stage_count; j++) {
			const struct bv_pipeline_stage *stage = &p->stage[j];

			if (stage->from.type == BV_NODE_GRAPH) {
				fprintf(stderr, "Error: Graph node %s 'from'\n",
						dpp__get_node_label(
								&stage->from));
				return false;
			}

			if (stage->to.type == BV_NODE_GRAPH) {
				if (!dpp__add_graph(ctx, stage)) {
					return false;
				}
			}
		}
	}

	return true;
}

/**
 * Print the internal representation of the data processing pipeline setup.
 */
static void dpp__internal_reprensetaion_print(void)
{
	for (unsigned i = 0; i < dpp_g.channel_count; i++) {
		fprintf(stderr, "DPP: Channel: Offset: %u\n",
				dpp_g.channel[i].dpp_offset);
	}

	for (unsigned i = 0; i < dpp_g.filter_count; i++) {
		fprintf(stderr, "DPP: Filter (%s)\n",
				dpp_g.filter[i].filter->filter);
		for (unsigned j = 0; j < dpp_g.filter[i].input_count; j++) {
			if (dpp_g.filter[i].input[j].set == false) {
				fprintf(stderr, "DPP: - input (%s): UNSET\n",
						dpp_g.filter[i].input[j].name);
				continue;
			}
			fprintf(stderr, "DPP: - input (%s): offset: %u\n",
					dpp_g.filter[i].input[j].name,
					dpp_g.filter[i].input[j].dpp_offset);
		}
		for (unsigned j = 0; j < dpp_g.filter[i].output_count; j++) {
			if (dpp_g.filter[i].output[j].set == false) {
				fprintf(stderr, "DPP: - output (%s): UNSET\n",
						dpp_g.filter[i].output[j].name);
				continue;
			}
			fprintf(stderr, "DPP: - output (%s): offset: %u\n",
					dpp_g.filter[i].output[j].name,
					dpp_g.filter[i].output[j].dpp_offset);
		}
	}

	for (unsigned i = 0; i < dpp_g.graph_count; i++) {
		fprintf(stderr, "DPP: Graph: Offset: %u\n",
				dpp_g.graph[i].dpp_offset);
	}
}

/**
 * Build the internal representaion for a setup.
 *
 * \param[in]  s  The setup to create.
 * \return true on success, false otherwise.
 */
static bool dpp__build_setup_internal_representation(
		const struct bv_setup *s)
{
	if (s == NULL) {
		fprintf(stderr, "Error: NULL setup.\n");
		return false;
	}

	if (!dpp__channel_scan(s)) {
		fprintf(stderr, "Error: DPP: Channel scan failed.\n");
		goto error;
	}

	if (!dpp__filter_scan(s)) {
		fprintf(stderr, "Error: DPP: Filter scan failed.\n");
		goto error;
	}

	if (!dpp__graph_scan(s)) {
		fprintf(stderr, "Error: DPP: Graph scan failed.\n");
		goto error;
	}

	dpp_g.pipeline_len = dpp_g.dpp_offset_next;
	dpp__internal_reprensetaion_print();
	return true;

error:
	dpp__cleanup();
	return false;
}

/* Exported interface, documented in dpp.h */
bool dpp_get_dpp_list(
		char ***list,
		unsigned *list_count)
{
	unsigned count = dpp_g.dpp->setup_count;
	char **strings = NULL;

	strings = calloc(count, sizeof(*strings));
	if (strings == NULL) {
		goto error;
	}

	for (unsigned i = 0; i < count; i++) {
		strings[i] = strdup(dpp_g.dpp->setup[i].name);
		if (strings[i] == NULL) {
			goto error;
		}
	}

	*list = strings;
	*list_count = count;
	return true;

error:
	util_free_string_vector(strings, count);
	return false;
}

/* Exported interface, documented in dpp.h */
bool dpp_start(
		unsigned frequency,
		unsigned dpp_index,
		struct bv_value **pipeline_out,
		unsigned *channels_out)
{
	struct bv_value *pipeline;

	dpp_g.dpp_offset_next = 0;
	dpp_g.frequency = frequency;

	if (dpp_index >= dpp_g.dpp->setup_count) {
		fprintf(stderr, "Pipeline index %u out of range "
				"(max: %"PRIu32").\n",
				dpp_index, dpp_g.dpp->setup_count);
		return false;
	}

	if (!filter_start(frequency)) {
		return false;
	}

	if (!dpp__build_setup_internal_representation(
			&dpp_g.dpp->setup[dpp_index])) {
		filter_finish();
		return false;
	}

	pipeline = calloc(dpp_g.pipeline_len, sizeof(*pipeline));
	if (pipeline == NULL) {
		dpp__cleanup();
		filter_finish();
		return false;
	}

	*pipeline_out = pipeline;
	*channels_out = dpp_g.channel_count;
	return true;
}

/* Exported interface, documented in dpp.h */
void dpp_stop(struct bv_value *pipeline)
{
	dpp__cleanup();
	filter_finish();

	free(pipeline);
}

/* Exported interface, documented in dpp.h */
bool dpp_process(struct bv_value *pipeline)
{
	if (!filter_proc(pipeline, dpp_g.pipeline_len)) {
		return false;
	}

	for (unsigned i = 0; i < dpp_g.graph_count; i++) {
		if (!graph_data_add(i, bv_value_unsigned(
				&pipeline[dpp_g.graph[i].dpp_offset]) - INT32_MAX)) {
			return false;
		}
	}

	return true;
}

/* Exported interface, documented in dpp.h */
enum bl_acq_flash_mode dpp_get_emission_mode(unsigned dpp_index)
{
	assert(dpp_index < dpp_g.dpp->setup_count);

	return dpp_g.dpp->setup[dpp_index].acq_mode;
}

/* Exported interface, documented in dpp.h */
uint16_t dpp_get_source_mask(unsigned dpp_index)
{
	const struct bv_setup *s;
	uint16_t mask = 0;

	assert(dpp_index < dpp_g.dpp->setup_count);
	s = &dpp_g.dpp->setup[dpp_index];

	for (unsigned i = 0; i < s->context_count; i++) {
		const struct bv_context *ctx = &s->context[i];

		for (unsigned j = 0; j < ctx->channel_count; j++) {
			mask |= 1u << ctx->channel[j].channel;
		}
	}

	return mask;
}
