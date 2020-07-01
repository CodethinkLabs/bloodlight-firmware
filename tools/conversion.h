#ifndef BL_CONVERSION_H
#define Bl_CONVERSION_H

#include <stdint.h>
#include <stdio.h>

uint16_t bl_count_channels(uint16_t src_mask);

#define MAX_SAMPLES 64

int bl_live_samples_to_xy(
		const union bl_msg_data *msg,
		FILE *file,
		long current_x[MSG_CHANNELS_MAX],
		int sample_rate);

int bl_single_mask_to_channel(uint16_t src_mask);
#endif /* BL_CONVERSION_H */