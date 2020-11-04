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

#include "dma.h"

#include <stdbool.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/dma.h>

#if (BL_REVISION >= 2)
#include <libopencm3/stm32/dmamux.h>
#endif



struct bl_acq_dma_s
{
	const uint32_t rcc_unit;
	const uint32_t base;

#if (BL_REVISION >= 2)
	const uint32_t dmamux;
	const uint32_t dmamux_offset;
	const uint32_t dmamux_rcc;
#endif

	bool           enable;
	uint8_t        channel_mask;
};

static bl_acq_dma_t bl_acq__dma1 =
{
	.rcc_unit      = RCC_DMA1,
	.base          = DMA1,
#if (BL_REVISION >= 2)
	.dmamux        = DMAMUX1,
	.dmamux_offset = 0,
	.dmamux_rcc    = RCC_DMAMUX1,
#endif
	.enable        = 0,
	.channel_mask  = 0x00,
};
bl_acq_dma_t *bl_acq_dma1 = &bl_acq__dma1;

#if (BL_REVISION == 1)
static bl_acq_dma_t bl_acq__dma2 =
{
	.rcc_unit      = RCC_DMA2,
	.base          = DMA2,
	.enable        = 0,
	.channel_mask  = 0x00,
};
bl_acq_dma_t *bl_acq_dma2 = &bl_acq__dma2;
#endif


void bl_acq_dma_enable(bl_acq_dma_t *dma)
{
	if (dma->enable) {
		/* Already enabled. */
		return;
	}

	rcc_periph_clock_enable(dma->rcc_unit);

#if (BL_REVISION >= 2)
	rcc_periph_clock_enable(dma->dmamux_rcc);
#endif

	dma->enable = true;
}

void bl_acq_dma_disable(bl_acq_dma_t *dma)
{
	/* Disable all channels. */
	for (unsigned i = 0; i < (sizeof(dma->channel_mask) * 8); i++) {
		if ((dma->channel_mask & (1U << i)) == 0) {
			continue;
		}

		dma_disable_transfer_complete_interrupt(dma->base, i);
		dma_disable_half_transfer_interrupt(dma->base, i);
		dma_disable_channel(dma->base, i);
	}
	dma->channel_mask = 0x00;

	/* Don't disable DMA or DMAMUX because they may be used elsewhere. */
}


void bl_acq_dma_channel_enable(bl_acq_dma_t *dma,
		unsigned channel, uint8_t dmareq,
		volatile void *m_addr, volatile void *p_addr, uint32_t count,
		bool is_adc_dma, enum dma_data_direction dir)
{
#if (BL_REVISION >= 2)
	dmareq += dma->dmamux_offset;

	dmamux_set_dma_channel_request(dma->dmamux, channel, dmareq);
#else
	(void)dmareq;
#endif

	dma_channel_reset(dma->base, channel);

	dma_set_peripheral_address(dma->base, channel, (uint32_t)p_addr);
	dma_set_memory_address(dma->base, channel, (uint32_t)m_addr);

	dma_enable_memory_increment_mode(dma->base, channel);

	if (dir == DMA_FROM_DEVICE) {
		dma_set_read_from_peripheral(dma->base, channel);
	} else if (dir == DMA_TO_DEVICE) {
		dma_set_read_from_memory(dma->base, channel);
	}

	dma_set_peripheral_size(dma->base, channel, DMA_CCR_PSIZE_16BIT);
	dma_set_memory_size(dma->base, channel, DMA_CCR_MSIZE_16BIT);

	dma_enable_transfer_complete_interrupt(dma->base, channel);

	dma_set_number_of_data(dma->base, channel, (count * 2));

	if (is_adc_dma) {
		dma_enable_half_transfer_interrupt(dma->base, channel);
		dma_enable_circular_mode(dma->base, channel);
	}

	dma_enable_channel(dma->base, channel);

	dma->channel_mask |= 1U << channel;
}
