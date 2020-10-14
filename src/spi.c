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

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/dma.h>

#if (BL_REVISION >= 2)
#include <libopencm3/stm32/dmamux.h>
#endif

#include "spi.h"
#include "led.h"

#if (BL_REVISION == 1)
static bl_spi_dma_t bl_spi_dma__rx =
{
	.dma         = &bl_acq_dma1,
	.dma_channel = DMA_CHANNEL4,
	.irq         = NVIC_DMA1_CHANNEL4_IRQ,
	.enable      = 0,
};

static bl_spi_dma_t bl_spi_dma__tx =
{
	.dma         = &bl_acq_dma1,
	.dma_channel = DMA_CHANNEL5,
	.irq         = NVIC_DMA1_CHANNEL5_IRQ,
	.enable      = 0,
};
#else
static bl_spi_dma_t bl_spi_dma__rx =
{
	.dma         = &bl_acq_dma1,
	.dma_channel = DMA_CHANNEL6,
	.dmamux_req  = DMAMUX_CxCR_DMAREQ_ID_SPI2_RX,
	.irq         = NVIC_DMA1_CHANNEL6_IRQ,
	.enable      = 0,
};

static bl_spi_dma_t bl_spi_dma__tx =
{
	.dma         = &bl_acq_dma1,
	.dma_channel = DMA_CHANNEL7,
	.dmamux_req  = DMAMUX_CxCR_DMAREQ_ID_SPI2_TX,
	.irq         = NVIC_DMA1_CHANNEL7_IRQ,
	.enable      = 0,
};
#endif

bl_spi_dma_t *bl_spi_dma_rx = &bl_spi_dma__rx;
bl_spi_dma_t *bl_spi_dma_tx = &bl_spi_dma__tx;

/* SPI receive completed with DMA */
void dma1_channel4_isr(void)
{
	if ((DMA1_ISR &DMA_ISR_TCIF2) != 0) {
		DMA1_IFCR |= DMA_IFCR_CTCIF2;
	}

	dma_disable_transfer_complete_interrupt(DMA1, DMA_CHANNEL4);

	spi_disable_rx_dma(SPI2);

	dma_disable_channel(DMA1, DMA_CHANNEL4);
}

/* SPI transmit completed with DMA */
void dma1_channel5_isr(void)
{
	if ((DMA1_ISR &DMA_ISR_TCIF3) != 0) {
		DMA1_IFCR |= DMA_IFCR_CTCIF3;
	}

	dma_disable_transfer_complete_interrupt(DMA1, DMA_CHANNEL5);

	spi_disable_tx_dma(SPI2);

	dma_disable_channel(DMA1, DMA_CHANNEL5);
}

void dma1_channel6_isr(void)
{
	if ((DMA1_ISR &DMA_ISR_TCIF2) != 0) {
		DMA1_IFCR |= DMA_IFCR_CTCIF2;
	}

	dma_disable_transfer_complete_interrupt(DMA1, DMA_CHANNEL6);

	spi_disable_rx_dma(SPI2);

	dma_disable_channel(DMA1, DMA_CHANNEL6);
}

/* SPI transmit completed with DMA */
void dma1_channel7_isr(void)
{
	if ((DMA1_ISR &DMA_ISR_TCIF3) != 0) {
		DMA1_IFCR |= DMA_IFCR_CTCIF3;
	}

	dma_disable_transfer_complete_interrupt(DMA1, DMA_CHANNEL7);

	spi_disable_tx_dma(SPI2);

	dma_disable_channel(DMA1, DMA_CHANNEL7);
}

static void bl_spi__setup(void)
{
	rcc_periph_clock_enable(RCC_SPI2);
	/* For spi signal pins */
	rcc_periph_clock_enable(RCC_GPIOB);
	/* For spi DMA */
	rcc_periph_clock_enable(RCC_DMA1);

	/* Setup GPIO pins for AF5 for SPI2 signals. */

	gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_PULLDOWN,
			GPIO13 | GPIO14 | GPIO15);
	gpio_set_af(GPIOB, GPIO_AF5, GPIO13 | GPIO14 | GPIO15);

	/* SPI initialization */
	spi_set_master_mode(SPI2);
	spi_set_baudrate_prescaler(SPI2, SPI_CR1_BR_FPCLK_DIV_64);
	spi_set_clock_polarity_0(SPI2);
	spi_set_clock_phase_0(SPI2);
	spi_set_full_duplex_mode(SPI2);
	spi_set_unidirectional_mode(SPI2); /* bidirectional but in 3-wire */
	spi_set_data_size(SPI2, SPI_CR2_DS_8BIT);
	spi_enable_software_slave_management(SPI2);
	spi_send_lsb_first(SPI2);
	spi_set_nss_high(SPI2);
	spi_fifo_reception_threshold_8bit(SPI2);
	SPI_I2SCFGR(SPI2) &= ~SPI_I2SCFGR_I2SMOD;
	spi_enable(SPI2);
}

/* Exported function, documented in spi.h */
int bl_spi_dma_transceive(uint8_t *tx_buf, uint8_t *rx_buf, int len)
{
	/* Reset SPI data register. */
	volatile uint8_t temp_data __attribute__ ((unused));
	while (SPI_SR(SPI2) & (SPI_SR_RXNE | SPI_SR_OVR)) {
		temp_data = SPI_DR(SPI2);
	}

	/* Set up rx dma, note it has higher priority to avoid overrun */
	dma_set_peripheral_address(DMA1, DMA_CHANNEL4, (uint32_t)&SPI2_DR);
	dma_set_memory_address(DMA1, DMA_CHANNEL4, (uint32_t)rx_buf);
	dma_set_number_of_data(DMA1, DMA_CHANNEL4, len);
	dma_set_read_from_peripheral(DMA1, DMA_CHANNEL4);
	dma_enable_memory_increment_mode(DMA1, DMA_CHANNEL4);
	dma_set_peripheral_size(DMA1, DMA_CHANNEL4, DMA_CCR_PSIZE_8BIT);
	dma_set_memory_size(DMA1, DMA_CHANNEL4, DMA_CCR_MSIZE_8BIT);
	dma_set_priority(DMA1, DMA_CHANNEL4, DMA_CCR_PL_VERY_HIGH);

	/* Set up tx dma */
	dma_set_peripheral_address(DMA1, DMA_CHANNEL5, (uint32_t)&SPI2_DR);
	dma_set_memory_address(DMA1, DMA_CHANNEL5, (uint32_t)tx_buf);
	dma_set_number_of_data(DMA1, DMA_CHANNEL5, len);
	dma_set_read_from_memory(DMA1, DMA_CHANNEL5);
	dma_enable_memory_increment_mode(DMA1, DMA_CHANNEL5);
	dma_set_peripheral_size(DMA1, DMA_CHANNEL5, DMA_CCR_PSIZE_8BIT);
	dma_set_memory_size(DMA1, DMA_CHANNEL5, DMA_CCR_MSIZE_8BIT);
	dma_set_priority(DMA1, DMA_CHANNEL5, DMA_CCR_PL_HIGH);

	/* Enable dma transfer complete interrupts */
	dma_enable_transfer_complete_interrupt(DMA1, DMA_CHANNEL4);
	dma_enable_transfer_complete_interrupt(DMA1, DMA_CHANNEL5);

	/* Activate dma channels */
	dma_enable_channel(DMA1, DMA_CHANNEL4);
	dma_enable_channel(DMA1, DMA_CHANNEL5);

	/* Enable the spi transfer via dma
	 * This will immediately start the transmission,
	 * after which when the receive is complete, the
	 * receive dma will activate
	 */
	spi_enable_rx_dma(SPI2);
	spi_enable_tx_dma(SPI2);

	return 0;
}

/* Exported function, documented in spi.h */
void bl_spi_dma_init(void)
{
	/* DMA IRQ setup, priority set to be lower than acq */
	/* SPI2 RX on DMA1 Channel 4 */
	nvic_set_priority(NVIC_DMA1_CHANNEL4_IRQ, 0);
	nvic_enable_irq(NVIC_DMA1_CHANNEL4_IRQ);
	/* SPI2 TX on DMA1 Channel 5 */
	nvic_set_priority(NVIC_DMA1_CHANNEL5_IRQ, 0);
	nvic_enable_irq(NVIC_DMA1_CHANNEL5_IRQ);
}

/* Exported function, documented in spi.h */
void bl_spi_init(void)
{
	bl_spi__setup();
}
