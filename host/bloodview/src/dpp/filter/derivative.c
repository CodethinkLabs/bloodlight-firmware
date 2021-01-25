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
 * \brief Implementation of the data processing pipeline derivative filter.
 */

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "../../util.h"

#include "../value.h"
#include "../filter.h"

#include "derivative.h"

/** Filter context. */
struct derivative_ctx {
	unsigned output; /** Output pipeline offset. */
	unsigned input;  /** Input pipeline offset */

	/** Previous value. */
	struct bv_value prev;
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
static filter_ctx filter_derivative__init(
		const struct bv_param *param,
		const unsigned *output,
		const unsigned *input,
		unsigned param_count,
		unsigned frequency,
		unsigned n_output,
		unsigned n_input)
{
	struct derivative_ctx *ctx;

	BV_UNUSED(param);
	BV_UNUSED(frequency);

	if (param_count != 0) {
		fprintf(stderr, "Error: Derivative: Bad parameter count: %u.\n",
				param_count);
		return NULL;
	}

	if (n_output != 1) {
		fprintf(stderr, "Error: Derivative: Bad output count: %u.\n",
				n_output);
		return NULL;
	}
	if (n_input != 1) {
		fprintf(stderr, "Error: Derivative: Bad input count: %u.\n",
				n_input);
		return NULL;
	}

	ctx = calloc(1, sizeof(*ctx));
	if (ctx == NULL) {
		return NULL;
	}

	ctx->output = output[0];
	ctx->input = input[0];
	ctx->prev.type_unsigned = INT_MAX;

	return ctx;
}

/**
 * Run the filter over the pipeline.
 *
 * \param[in] ctx           A filter instance.
 * \param[in] pipeline      The data pipeline.
 * \param[in] pipeline_len  The length of the pipeline.
 */
static bool filter_derivative__proc(
		filter_ctx ctx,
		struct bv_value *pipeline,
		size_t pipeline_len)
{
	struct derivative_ctx *deriv_ctx = ctx;

	assert(deriv_ctx->input  < pipeline_len);
	assert(deriv_ctx->output < pipeline_len);
	assert(pipeline[deriv_ctx->input].type == BV_VALUE_UNSIGNED);

	BV_UNUSED(pipeline_len);

	pipeline[deriv_ctx->output].type = BV_VALUE_UNSIGNED;
	pipeline[deriv_ctx->output].type_unsigned =
			INT_MAX + pipeline[deriv_ctx->input].type_unsigned -
			deriv_ctx->prev.type_unsigned;

	deriv_ctx->prev = pipeline[deriv_ctx->input];

	return true;
}

/**
 * Destroy a filter instance.
 *
 * \param[in] ctx  A filter instance.
 */
static void filter_derivative__fini(
		filter_ctx ctx)
{
	struct derivative_ctx *deriv_ctx = ctx;

	free(deriv_ctx);
}

/* Exported function, documented in filter/derivative.h */
bool filter_derivative_register(void)
{
	return filter_register("Derivative",
			filter_derivative__init,
			filter_derivative__proc,
			filter_derivative__fini);
}
