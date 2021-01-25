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
 * \brief Implementation of the data processing pipeline filter handling.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "filter.h"

/**
 * Filter implementation.
 */
struct filter_impl {
	const char *name;    /**< Filter's name */
	filter_init_cb init; /**< Filter's initialisation function */
	filter_proc_cb proc; /**< Filter's sample processing function */
	filter_fini_cb fini; /**< Filter's finalisation function */
};

/**
 * Filter sequence entry.
 */
struct filter_entry {
	filter_ctx ctx;                 /**< Filter's context. */
	const struct filter_impl *impl; /**< Filter's implementation. */
};

struct {
	struct filter_impl *implementation;
	unsigned implementation_count;

	struct filter_entry *filter;
	unsigned filter_count;

	unsigned frequency;
} filter_g;

/* Exported function, documented in filter.h */
void filter_fini(void)
{
	filter_finish();

	free(filter_g.implementation);
	filter_g.implementation = NULL;
	filter_g.implementation_count = 0;
}

/* Exported function, documented in filter.h */
bool filter_init(void)
{
	return true;
}

/**
 * Search for the implementation of a given filter by name.
 *
 * \param[in]  name  Filter name to look up.
 * \return Pointer to filter implementation, or NULL on error.
 */
static struct filter_impl *filter__lookup_impl(const char *name)
{
	for (unsigned i = 0; i < filter_g.implementation_count; i++) {
		if (strcmp(filter_g.implementation[i].name, name) == 0) {
			return &filter_g.implementation[i];
		}
	}

	return NULL;
}

/* Exported function, documented in filter.h */
bool filter_register(
		const char *name,
		filter_init_cb init,
		filter_proc_cb proc,
		filter_fini_cb fini)
{
	struct filter_impl *impl;
	unsigned count;

	impl = filter__lookup_impl(name);
	if (impl != NULL) {
		fprintf(stderr, "Error: %s filter already registered.\n", name);
		return false;
	}

	count = filter_g.implementation_count;
	impl = realloc(filter_g.implementation, (count + 1) * sizeof(*impl));
	if (impl == NULL) {
		return false;
	}

	impl[count].name = name;
	impl[count].init = init;
	impl[count].proc = proc;
	impl[count].fini = fini;

	filter_g.implementation_count++;
	filter_g.implementation = impl;

	return true;
}

/* Exported function, documented in filter.h */
bool filter_start(
		unsigned frequency)
{
	filter_g.frequency = frequency;

	return true;
}

/* Exported function, documented in filter.h */
bool filter_add(
		const char *name,
		const struct bv_param *param,
		const unsigned *output,
		const unsigned *input,
		unsigned param_count,
		unsigned n_output,
		unsigned n_input)
{
	const struct filter_impl *impl;
	struct filter_entry *filter;
	unsigned count;
	filter_ctx ctx;

	impl = filter__lookup_impl(name);
	if (impl == NULL) {
		fprintf(stderr, "Error: %s filter not found.\n", name);
		return false;
	}

	ctx = impl->init(param, output, input,
			param_count, filter_g.frequency,
			n_output, n_input);
	if (ctx == NULL) {
		return false;
	}

	count = filter_g.filter_count;
	filter = realloc(filter_g.filter, (count + 1) * sizeof(*filter));
	if (filter == NULL) {
		impl->fini(ctx);
		return false;
	}

	filter[count].ctx = ctx;
	filter[count].impl = impl;

	filter_g.filter_count++;
	filter_g.filter = filter;

	return true;
}

/* Exported function, documented in filter.h */
bool filter_proc(
		struct bv_value *pipeline,
		size_t pipeline_len)
{
	for (unsigned i = 0; i < filter_g.filter_count; i++) {
		const struct filter_entry *filter = &filter_g.filter[i];
		const struct filter_impl *impl = filter->impl;

		if (!impl->proc(filter->ctx, pipeline, pipeline_len)) {
			return false;
		}
	}

	return true;
}

/* Exported function, documented in filter.h */
void filter_finish(void)
{
	if (filter_g.filter != NULL) {
		for (unsigned i = 0; i < filter_g.filter_count; i++) {
			filter_g.filter[i].impl->fini(
					filter_g.filter[i].ctx);
		}
		free(filter_g.filter);
		filter_g.filter = NULL;
		filter_g.filter_count = 0;
	}
}
