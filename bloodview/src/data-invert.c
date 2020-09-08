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
 * \brief Implementation of data inversion.
 *
 * This module inverts samples, flipping the data upside-down.
 */

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include "data-invert.h"
#include "util.h"

/** Inversion filter context. */
struct data_invert_ctx {
	unsigned invert; /**< Mask of channels in invert. */
	unsigned count;  /**< Number of channels. */
};

/* Exported interface, documented in data-invert.h */
void data_invert_fini(void *pw)
{
	struct data_invert_ctx *ctx = pw;

	free(ctx);
}

/* Exported interface, documented in data-invert.h */
void *data_invert_init(
		const struct data_invert_config *config,
		unsigned frequency,
		unsigned channels,
		uint32_t src_mask)
{
	struct data_invert_ctx *ctx;
	unsigned index;

	BV_UNUSED(frequency);

	ctx = calloc(1, sizeof(*ctx));
	if (ctx == NULL) {
		return NULL;
	}

	index = 0;
	for (unsigned i = 0; i < BL_ACQ__SRC_COUNT; i++) {
		if (src_mask & (1u << i)) {
			if (config->invert[index] == true) {
				ctx->invert |= (1u << index);
			}
			index++;
		}
	}

	ctx->count = channels;
	return ctx;
}

/* Exported interface, documented in data-invert.h */
uint32_t data_invert_proc(
		void *pw,
		unsigned channel,
		uint32_t sample)
{
	struct data_invert_ctx *ctx = pw;

	assert(channel < ctx->count);

	if (ctx->invert & (1u << channel)) {
		sample = UINT32_MAX - sample;
	}

	return sample;
}
