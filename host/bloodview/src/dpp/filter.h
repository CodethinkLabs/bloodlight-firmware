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
 * \brief Interface to the data processing pipeline filter handling.
 */

#ifndef BV_DPP_FILTER_H
#define BV_DPP_FILTER_H

#include <stddef.h>
#include <stdbool.h>

struct bv_param;
struct bv_value;

/** A filter instance. */
typedef void * filter_ctx;

/**
 * Create a filter instance.
 *
 * This is a filter implementation callback table entry.  It gets called to
 * create an instance of a registered filter.  Anyone implementing a new
 * filter must implement this.
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
typedef filter_ctx (* filter_init_cb)(
		const struct bv_param *param,
		const unsigned *output,
		const unsigned *input,
		unsigned param_count,
		unsigned frequency,
		unsigned n_output,
		unsigned n_input);

/**
 * Run the filter over the pipeline.
 *
 * This is a filter implementation callback table entry.  It gets called to
 * process the filter's sample data.  Anyone implementing a new filter
 * must implement this.
 *
 * It reads inputs and write outputs to the pipeline data array, at offsets
 * given at filter initialisation.
 *
 * \param[in] ctx           A filter instance.
 * \param[in] pipeline      The data pipeline.
 * \param[in] pipeline_len  The length of the pipeline.
 */
typedef bool (* filter_proc_cb)(
		filter_ctx ctx,
		struct bv_value *pipeline,
		size_t pipeline_len);

/**
 * Destroy a filter instance.
 *
 * This is a filter implementation callback table entry.  It gets called to
 * destry and instance of a registered filter.  Anyone implementing a new filter
 * must implement this.
 *
 * \param[in] ctx  A filter instance.
 */
typedef void (* filter_fini_cb)(
		filter_ctx ctx);

/**
 * Initialise the filter module.
 *
 * Call this before any other filter_* functions.
 *
 * \return true on success, or false on error.
 */
bool filter_init(void);

/**
 * Register the existence of a filter.
 *
 * This can be called once on startup to register a filter.
 *
 * \param[in]  name  Filter's name.
 * \param[in]  init  Filter's initialisation function.
 * \param[in]  proc  Filter's sample processing function.
 * \param[in]  fini  Filter's finalisation function.
 * \return true on success, or false on error.
 */
bool filter_register(
		const char *name,
		filter_init_cb init,
		filter_proc_cb proc,
		filter_fini_cb fini);

/**
 * Finalise the filter module.
 *
 * Don't call any other filter_* functions after this, except \ref filter_init.
 */
void filter_fini(void);

/**
 * Prepare the filter module for an acquisition.
 *
 * \param[in]  frequency  The frequency of the acquisition run.
 * \return true on success, or false on error.
 */
bool filter_start(
		unsigned frequency);

/**
 * Add a filter to the filtering sequence.
 *
 * A filter of this name must have been registered with \ref filter_register.
 *
 * The input and output arrays are valid until \ref filter_finish is called,
 * so they can be referred to during \ref filter_proc.
 *
 * Inputs and outputs are in the order that the inputs and outputs are
 * listed in the filter specification YAML.
 *
 * \param[in]  name         The name of the registered filter to add.
 * \param[in]  param        Array of filter parameter/values.
 * \param[in]  output       Array of pipeline value offsets for outputs.
 * \param[in]  input        Array of pipeline value offsets for inputs.
 * \param[in]  param_count  Number of parameters.
 * \param[in]  n_output     Number of outputs.
 * \param[in]  n_input      Number of inputs.
 * \return true on success, or false on error.
 */
bool filter_add(
		const char *name,
		const struct bv_param *param,
		const unsigned *output,
		const unsigned *input,
		unsigned param_count,
		unsigned n_output,
		unsigned n_input);


/**
 * Run the registered filters over the pipeline.
 *
 * \param[in,out]  pipeline      Pipeline values.
 * \param[in]      pipeline_len  Max pipeline value offset.
 * \return true on success, or false on error.
 */
bool filter_proc(
		struct bv_value *pipeline,
		size_t pipeline_len);

/**
 * Cleanup the filter module after an acquisition.
 *
 * \return true on success, or false on error.
 */
void filter_finish(void);

#endif
