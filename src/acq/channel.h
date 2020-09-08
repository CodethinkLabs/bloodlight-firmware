#ifndef BL_ACQ_CHANNEL_H
#define BL_ACQ_CHANNEL_H

#include <stdbool.h>
#include <stdint.h>

#include "../error.h"
#include "../acq.h"

#define BL_ACQ_CHANNEL_COUNT 16

enum bl_error bl_acq_channel_configure(unsigned channel,
		enum bl_acq_source source,
		bool sample32, uint32_t sw_offset, uint8_t sw_shift);

void bl_acq_channel_enable(unsigned channel);
void bl_acq_channel_disable(unsigned channel);

bool bl_acq_channel_is_enabled(unsigned channel);

enum bl_acq_source bl_acq_channel_get_source(unsigned channel);

void bl_acq_channel_commit_sample(unsigned channel, uint32_t sample);

#endif
