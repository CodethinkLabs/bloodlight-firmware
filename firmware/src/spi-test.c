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

#include <libopencm3/stm32/spi.h>

#include "bl.h"
#include "spi.h"
#include "led.h"
#include "acq.h"

static void bl_spi_simply_test(void)
{
	uint8_t tx_counter = 0;

	bl_spi_init();
	bl_led_set(0xFFFF);
	while (1)
	{
		spi_send8(SPI2, tx_counter);
		uint8_t rx_counter = spi_read8(SPI2);
		for (int i = 0; i < 2000000; i++)
		{
			__asm__("nop");
		}

		bl_led_set(rx_counter << 8);
		tx_counter++;
	}
}

static void bl_spi_dma_test(void)
{
    uint8_t counter = 16;
    uint8_t tx_packet[16], rx_packet[16];
    int i;

    for (i = 0; i < counter; i++)
    {
        tx_packet[i] = counter - i;
        rx_packet[i] = 0x50;
    }

    bl_spi_init();
	bl_spi_dma_init();
    while (1)
    {
        bl_spi_dma_transceive(tx_packet, rx_packet, counter);

        while (!(SPI_SR(SPI2) & SPI_SR_TXE))
            ;
        while (SPI_SR(SPI2) & SPI_SR_BSY)
            ;

        for (int j = 0; j < counter; j++)
        {
            bl_led_set(rx_packet[j] << 8);

            for (i = 0; i < 20000000; i++)
            {
                __asm__("nop");
            }
        }

        for (i = 0; i < 16; i++)
        {
            rx_packet[i] = 0x50;
        }
    }
}

int main(void)
{
	bl_spi_mode = BL_ACQ_SPI_MOTHER;
	bl_init();
#ifdef BL_SPI_DMA_TEST
	bl_spi_dma_test();
#else
	bl_spi_simply_test();
#endif
}
