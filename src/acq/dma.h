#ifndef BL_ACQ_DMA_H
#define BL_ACQ_DMA_H

#include <stdint.h>

typedef struct bl_acq_dma_s bl_acq_dma_t;

extern bl_acq_dma_t *bl_acq_dma1;

#if (BL_REVISION == 1)
extern bl_acq_dma_t *bl_acq_dma2;
#endif


void bl_acq_dma_enable(bl_acq_dma_t *dma);
void bl_acq_dma_disable(bl_acq_dma_t *dma);

void bl_acq_dma_channel_enable(bl_acq_dma_t *dma,
		unsigned channel, uint8_t dmareq,
		volatile void *dst, volatile void *src, uint32_t count);

#endif

