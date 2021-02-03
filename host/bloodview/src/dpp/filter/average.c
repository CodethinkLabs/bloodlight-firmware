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
 * \brief Implementation of the data processing pipeline averaging filter.
 */

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "../../../common/fifo.h"

#include "../../util.h"

#include "../param.h"
#include "../filter.h"

#include "average.h"

/** Filter context. */
struct average_ctx {
	unsigned output; /** Output pipeline offset. */
	unsigned input;  /** Input pipeline offset */

	struct fifo *fifo; /** Sample record. */
	uint64_t sum;      /** Current sum. */
	bool normalise;    /** Whether to normalise values. */
};

/**
 * Create a filter instance.
 *
 * The input and output arrays are valid until \ref filter_finish is called,
 * so they can be referred to during \ref filter_proc.
 *
 * Inputs and outputs are in the order that the inputs and outputs are
 * listed in the filter specification YAML.
 *
 * \param[in]  param        Array of filter parameter/values.
 * \param[in]  output       Array of pipeline value offsets for outputs.
 * \param[in]  input        Array of pipeline value offsets for inputs.
 * \param[in]  param_count  Number of parameters.
 * \param[in]  frequency    The acquisition sampling rate.
 * \param[in]  n_output     Number of outputs.
 * \param[in]  n_input      Number of inputs.
 * \return A filter instance on success, of NULL on failure.
 */
static filter_ctx filter_average__init(
		const struct bv_param *param,
		const unsigned *output,
		const unsigned *input,
		unsigned param_count,
		unsigned frequency,
		unsigned n_output,
		unsigned n_input)
{
	unsigned capacity;
	struct average_ctx *ctx;
	const struct bv_param *param_hz;
	const struct bv_param *param_normalise;

	if (n_output != 1) {
		fprintf(stderr, "Error: Average: Bad output count: %u.\n",
				n_output);
		return NULL;
	}
	if (n_input != 1) {
		fprintf(stderr, "Error: Average: Bad input count: %u.\n",
				n_input);
		return NULL;
	}

	param_hz = param_lookup(param, param_count,
			"frequency", BV_VALUE_DOUBLE);
	if (param_hz == NULL) {
		return NULL;
	}

	param_normalise = param_lookup(param, param_count,
				"normalise", BV_VALUE_BOOL);
	if (param_normalise == NULL) {
		return NULL;
	}

	capacity = frequency / bv_value_double(&param_hz->value);

	ctx = calloc(1, sizeof(*ctx));
	if (ctx == NULL) {
		return NULL;
	}

	ctx->fifo = fifo_create(capacity, sizeof(struct bv_value));
	if (ctx->fifo == NULL) {
		free(ctx);
		return NULL;
	}

	ctx->normalise = bv_value_bool(&param_normalise->value);
	ctx->output = output[0];
	ctx->input = input[0];
	ctx->sum = 0;

	return ctx;
}

/**
 * Get a average sample from a channel.
 *
 * \param[in] ctx           A filter instance.
 * \return Normalised sample.
 */
static inline unsigned filter_average__get_average(
		const struct average_ctx *ctx)
{
	return ctx->sum / ctx->fifo->used;
}

/**
 * Get a normalised sample from a channel.
 *
 * \param[in] ctx     A filter instance.
 * \param[in] sample  Sample to normalise.
 * \return Normalised sample.
 */
static inline unsigned filter_average__get_normalised(
		const struct average_ctx *ctx,
		const struct bv_value *value)
{
	return INT_MAX + bv_value_unsigned(value) -
			filter_average__get_average(ctx);
}

/**
 * Write a sample to a channel buffer.
 *
 * \param[in]  c      Channel object.
 * \param[in]  value  Value to write to channel buffer.
 */
static inline void filter_average__add_sample(
		struct average_ctx *ctx,
		const struct bv_value *value)
{
	assert(ctx->fifo->used < ctx->fifo->capacity);

	if (!fifo_write(ctx->fifo, value)) {
		assert(0 && "filter_average__add_sample: fifo_write failed");
	}

	ctx->sum += bv_value_unsigned(value);
}

/**
 * Drop a sample from a channel buffer.
 *
 * \param[in]  c  Channel object.
 */
static inline void filter_average__drop_sample(
		struct average_ctx *ctx)
{
	struct bv_value old;

	assert(ctx->fifo->used > 0);

	if (!fifo_read(ctx->fifo, &old)) {
		assert(0 && "filter_average__drop_sample: fifo_read failed");
	}

	ctx->sum -= bv_value_unsigned(&old);
}

/**
 * Run the filter over the pipeline.
 *
 * \param[in] ctx           A filter instance.
 * \param[in] pipeline      The data pipeline.
 * \param[in] pipeline_len  The length of the pipeline.
 */
static bool filter_average__proc(
		filter_ctx ctx,
		struct bv_value *pipeline,
		size_t pipeline_len)
{
	struct average_ctx *avg_ctx = ctx;
	unsigned value;

	assert(avg_ctx->input  < pipeline_len);
	assert(avg_ctx->output < pipeline_len);

	BV_UNUSED(pipeline_len);

	filter_average__add_sample(avg_ctx, &pipeline[avg_ctx->input]);

	if (avg_ctx->normalise) {
		value = filter_average__get_normalised(avg_ctx,
				&pipeline[avg_ctx->input]);
	} else {
		value = filter_average__get_average(avg_ctx);
	}

	pipeline[avg_ctx->output].type = BV_VALUE_UNSIGNED;
	pipeline[avg_ctx->output].type_unsigned = value;

	if (avg_ctx->fifo->used == avg_ctx->fifo->capacity) {
		filter_average__drop_sample(avg_ctx);
	}

	return true;
}

/**
 * Destroy a filter instance.
 *
 * \param[in] ctx  A filter instance.
 */
static void filter_average__fini(
		filter_ctx ctx)
{
	if (ctx != NULL) {
		struct average_ctx *avg_ctx = ctx;
		fifo_destroy(avg_ctx->fifo);
		free(avg_ctx);
	}
}

/* Exported function, documented in filter/average.h */
bool filter_average_register(void)
{
	return filter_register("Average",
			filter_average__init,
			filter_average__proc,
			filter_average__fini);
}
