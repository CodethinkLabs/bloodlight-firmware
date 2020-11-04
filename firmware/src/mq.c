#include <stdint.h>
#include <stddef.h>

#include "common/util.h"

#include "mq.h"

#define BL_MSG_QUEUE_COUNT 76

#if (BL_MSG_QUEUE_COUNT % BL_ACQ_CHANNEL_COUNT)
#warning "Message queue size should be a multiple of channel count"
#endif

#if (BL_MSG_QUEUE_COUNT / BL_ACQ_CHANNEL_COUNT < 2)
#error "Message queue must have at least two queues per channel."
#endif

static union bl_msg_data msg[BL_MSG_QUEUE_COUNT];
struct mq_ctx mq_ctx[BL_ACQ_CHANNEL_COUNT];

volatile uint32_t mq_pending;

void bl_mq_init(void)
{
	uint8_t max = BL_ARRAY_LEN(msg) / BL_ARRAY_LEN(mq_ctx);

	for (unsigned i = 0; i < BL_ARRAY_LEN(mq_ctx); i++) {
		mq_ctx[i].msg   = &msg[i * max];
		mq_ctx[i].max   = max;
		mq_ctx[i].read  = 0;
		mq_ctx[i].write = 0;
	}

	mq_pending = 0x00;
}
