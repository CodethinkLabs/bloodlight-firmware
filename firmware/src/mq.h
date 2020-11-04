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

#ifndef BL_MQ_H
#define BL_MQ_H

#include "common/msg.h"

#include "acq/channel.h"

struct mq_ctx {
	union bl_msg_data *msg;
	uint8_t            max;

	volatile uint8_t read;
	volatile uint8_t write;
};

extern struct mq_ctx mq_ctx[BL_ACQ_CHANNEL_COUNT];
extern volatile uint32_t mq_pending;

void bl_mq_init(void);

static inline uint8_t bl_mq__advance(uint8_t max, uint8_t pos)
{
	pos++;
	return (pos >= max) ? 0 : pos;
}

static inline union bl_msg_data *bl_mq_acquire(unsigned channel)
{
	struct mq_ctx *ctx = &mq_ctx[channel];
	return &ctx->msg[ctx->write];
}

static inline void bl_mq_commit(unsigned channel)
{
	struct mq_ctx *ctx = &mq_ctx[channel];
	ctx->write = bl_mq__advance(ctx->max, ctx->write);
	/* Assuming use of ORR makes this atomic. */
	mq_pending |= 1U << channel;
}

static inline union bl_msg_data *bl_mq_peek(unsigned channel)
{
	struct mq_ctx *ctx = &mq_ctx[channel];
	return &ctx->msg[ctx->read];
}

static inline void bl_mq_release(unsigned channel)
{
	struct mq_ctx *ctx = &mq_ctx[channel];
	ctx->read = bl_mq__advance(ctx->max, ctx->read);
	if (ctx->read == ctx->write) {
		/* Assuming use of BIC makes this atomic. */
		mq_pending &= ~(1U << channel);
	}
}

static inline unsigned bl_mq_pending_channel(void)
{
	return (sizeof(unsigned) * 8) - 1 - __builtin_clz(mq_pending);
}

#endif
