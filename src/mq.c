#include <stdint.h>
#include <stddef.h>

#include "mq.h"

#define BL_MSG_QUEUE_COUNT 64

static union bl_msg_data msg[BL_MSG_QUEUE_COUNT];
struct mq_ctx mq_ctx[BL_ACQ__SRC_COUNT];
volatile unsigned mq_pending;

void bl_mq_init(void)
{
	uint8_t max = BL_MSG_QUEUE_COUNT / BL_ACQ__SRC_COUNT;

	for (unsigned i = 0; i < BL_ACQ__SRC_COUNT; i++) {
		mq_ctx[i].msg   = &msg[i * max];
		mq_ctx[i].max   = max;
		mq_ctx[i].read  = 0;
		mq_ctx[i].write = 0;
	}

	mq_pending = 0x00;
}
