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

#ifndef BL_SPI_H
#define BL_SPI_H

/**
 * Initialise the SPI module.
 */
void bl_spi_init(void);

/**
 * Initialise the DMA IRQs to be used by SPI module.
 */
void bl_spi_dma_init(void);

/**
 * Send data through SPI with DMA
 * \param[in]  tx_buf  transmission buffer address.
 * \param[in]  rx_buf  receive buffer address.
 * \return \ref 0 on success
 */
int bl_spi_dma_transceive(uint8_t *tx_buf, uint8_t *rx_buf, int len);

#endif