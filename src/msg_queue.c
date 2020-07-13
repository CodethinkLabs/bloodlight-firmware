#include "msg_queue.h"
#include <stdint.h>
#include <stddef.h>

#define BL_MSG_QUEUE_COUNT 64
#define BL_MSG_QUEUE_END 0xFF

static union bl_msg_data bl_msg_queue[BL_MSG_QUEUE_COUNT] = {0};

static volatile uint8_t bl_msg_queue_next[BL_MSG_QUEUE_COUNT] = {0};
static volatile uint8_t bl_msg_queue_alloc_head = BL_MSG_QUEUE_END;
static volatile uint8_t bl_msg_queue_alloc_tail = BL_MSG_QUEUE_END;
static volatile uint8_t bl_msg_queue_free  = 0;


void bl_msg_queue_init(void)
{
	bl_msg_queue_alloc_head = BL_MSG_QUEUE_END;
	bl_msg_queue_alloc_tail = BL_MSG_QUEUE_END;
	bl_msg_queue_free = 0;

	for (unsigned i = 0; i < (BL_MSG_QUEUE_COUNT - 1); i++) {
		bl_msg_queue_next[i] = (i + 1);
	}
	bl_msg_queue_next[BL_MSG_QUEUE_COUNT - 1] = BL_MSG_QUEUE_END;
}

static inline unsigned bl_msg_queue_index_from_ptr(union bl_msg_data *msg)
{
	return ((uintptr_t)msg - (uintptr_t)bl_msg_queue)
			/ sizeof(union bl_msg_data);
}


union bl_msg_data *bl_msg_queue_acquire(void)
{
	if (bl_msg_queue_free == BL_MSG_QUEUE_END) {
		return NULL;
	}

	uint8_t index = bl_msg_queue_free;
	bl_msg_queue_free = bl_msg_queue_next[bl_msg_queue_free];

	bl_msg_queue_next[index] = BL_MSG_QUEUE_END;
	return &bl_msg_queue[index];
}

void bl_msg_queue_commit(union bl_msg_data *msg)
{
	unsigned index = bl_msg_queue_index_from_ptr(msg);

	if (bl_msg_queue_alloc_tail == BL_MSG_QUEUE_END) {
		bl_msg_queue_next[index] = BL_MSG_QUEUE_END;
		bl_msg_queue_alloc_head = index;
		bl_msg_queue_alloc_tail = index;
	}

	bl_msg_queue_next[bl_msg_queue_alloc_tail] = index;
	bl_msg_queue_alloc_tail = index;
}

union bl_msg_data *bl_msg_queue_peek(void)
{
	if (bl_msg_queue_alloc_head == BL_MSG_QUEUE_END) {
		return NULL;
	}

	return &bl_msg_queue[bl_msg_queue_alloc_head];
}

void bl_msg_queue_release(union bl_msg_data *msg)
{
	unsigned index = bl_msg_queue_index_from_ptr(msg);

	/* Circular links will occur  if a packet in the alloc or free list
	 * is released, but the head of the alloc list or acquired messages
	 * may be freed. No checks for this as they'd be too expensive. */

	if (index == bl_msg_queue_alloc_head) {
		bl_msg_queue_alloc_head = bl_msg_queue_next[index];
	}

	if (index == bl_msg_queue_alloc_tail) {
		bl_msg_queue_alloc_tail = bl_msg_queue_next[index];
	}

	bl_msg_queue_next[index] = bl_msg_queue_free;
	bl_msg_queue_free = index;
}
