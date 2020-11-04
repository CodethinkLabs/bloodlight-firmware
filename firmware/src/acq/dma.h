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

#ifndef BL_ACQ_DMA_H
#define BL_ACQ_DMA_H

#include <stdbool.h>
#include <stdint.h>

enum dma_data_direction {
    DMA_TO_DEVICE,
    DMA_FROM_DEVICE,
    DMA_NONE,
};

typedef struct bl_acq_dma_s bl_acq_dma_t;

extern bl_acq_dma_t *bl_acq_dma1;

#if (BL_REVISION == 1)
extern bl_acq_dma_t *bl_acq_dma2;
#endif


void bl_acq_dma_enable(bl_acq_dma_t *dma);
void bl_acq_dma_disable(bl_acq_dma_t *dma);

void bl_acq_dma_channel_enable(bl_acq_dma_t *dma,
		unsigned channel, uint8_t dmareq,
		volatile void *m_addr, volatile void *p_addr, uint32_t count,
		bool is_adc_dma, enum dma_data_direction dir);
#endif

