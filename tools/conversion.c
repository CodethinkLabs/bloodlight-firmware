#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../src/error.h"
#include "../src/util.h"
#include "../src/msg.h"

#include "conversion.h"

#define MAX_SAMPLES 64

int bl_live_samples_to_xy(
		const union bl_msg_data *msg,
		FILE *file,
		long current_x[MSG_CHANNELS_MAX],
		int frequency)
{
	if (msg->type != BL_MSG_SAMPLE_DATA) {
		return -1;
	}

	int current_channel = bl_single_mask_to_channel(msg->sample_data.src_mask);
	if (current_channel == -1)
	{
		return -1;
	}
	float period = 1000 / frequency;
		
	for (int i = 0; i < msg->sample_data.count; i++)
	{
		float x_ms = period * ((current_x)[current_channel]);
		uint16_t sample = msg->sample_data.data[i];
		fprintf(file, "%d,%f,%d\n", current_channel, x_ms, sample);
		(current_x)[current_channel]++;
	}
	fflush(file);
	return 0;
}

/* 
 * Find a single channel index from a mask
 * Returns -1 if there isn't exactly one channel
 */
int bl_single_mask_to_channel(uint16_t src_mask)
{
	int channel = -1;
	bool set = false;

	for (unsigned i = 0; i < 16; i++) {
		if (src_mask & (1 << i)) {
			if (!set)
			{
				channel = i;
			}
			else
			{
				return -1;
			}
		}
	}
	return channel;
}

uint16_t bl_count_channels(uint16_t src_mask)
{
	uint16_t bit_count = 0;

	for (unsigned i = 0; i < 16; i++) {
		if (src_mask & (1 << i)) {
			bit_count++;
		}
	}

	return bit_count;
}