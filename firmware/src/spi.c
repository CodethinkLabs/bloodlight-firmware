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
#include "acq.h"

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

/* This is a macro to avoid repeated code for each ISR. */
#define DMA_CHANNEL_ISR(__dma, __channel) \
    void dma##__dma##_channel##__channel##_isr(void) \
    { \
	if ((DMA##__dma##_ISR & DMA_ISR_TCIF2) != 0) { \
		DMA##__dma##_IFCR |= DMA_IFCR_CTCIF2; \
	} \
	dma_disable_transfer_complete_interrupt(DMA##__dma, \
								DMA_CHANNEL##__channel); \
	spi_disable_rx_dma(SPI2); \
	dma_disable_channel(DMA##__dma, DMA_CHANNEL##__channel); \
    }

#if (BL_REVISION == 1)
/* SPI receive completed with DMA */
DMA_CHANNEL_ISR(1, 4);
/* SPI transmit completed with DMA */
DMA_CHANNEL_ISR(1, 5);
#else
/* SPI receive completed with DMA */
DMA_CHANNEL_ISR(1, 6);
/* SPI transmit completed with DMA */
DMA_CHANNEL_ISR(1, 7);
#endif

#define SPI_SR_FTLVL	SPI_SR_FTLVL_FIFO_FULL
#define SPI_SR_FRLVL	SPI_SR_FRLVL_FIFO_FULL
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

	/* Disable SPI before reset it
	 * Follow the procedure of disabling SPI documented
	 * at section 30.5.8 of TRM.
	 */
	while (SPI_SR(SPI2) & (SPI_SR_FTLVL | SPI_SR_BSY)) {
		;
	}
	spi_disable(SPI2);
	volatile uint8_t temp_data __attribute__ ((unused));
	while (SPI_SR(SPI2) & SPI_SR_FRLVL) {
		temp_data = SPI_DR(SPI2);
	}

	/* SPI initialization */
	spi_set_baudrate_prescaler(SPI2, SPI_CR1_BR_FPCLK_DIV_64);
	spi_set_clock_polarity_0(SPI2);
	spi_set_clock_phase_0(SPI2);
	spi_set_full_duplex_mode(SPI2);
	spi_set_unidirectional_mode(SPI2); /* bidirectional but in 3-wire */
	spi_set_data_size(SPI2, SPI_CR2_DS_8BIT);
	spi_enable_software_slave_management(SPI2);
	spi_send_lsb_first(SPI2);
	spi_fifo_reception_threshold_8bit(SPI2);
	SPI_I2SCFGR(SPI2) &= ~SPI_I2SCFGR_I2SMOD;
	if (bl_spi_mode == BL_ACQ_SPI_DAUGHTER ||
		bl_spi_mode == BL_ACQ_SPI_INIT) {
		/* Software NSS was used for SPI, and GPIO12 is used
		 * as generic GPIO to indicate LED flash
		 */
		gpio_mode_setup(GPIOB, GPIO_MODE_INPUT,
						GPIO_PUPD_PULLDOWN, GPIO12);

		spi_set_nss_low(SPI2);
		spi_set_slave_mode(SPI2);
	} else if (bl_spi_mode == BL_ACQ_SPI_MOTHER) {
		gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT,
						GPIO_PUPD_PULLDOWN, GPIO12);

		spi_set_nss_high(SPI2);
		spi_set_master_mode(SPI2);
	}

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

	bl_acq_dma_channel_enable(*(bl_spi_dma_rx->dma),
							  bl_spi_dma_rx->dma_channel,
							  bl_spi_dma_rx->dmamux_req,
							  rx_buf, &SPI2_DR, len/2,
							  false, DMA_FROM_DEVICE);

	bl_acq_dma_channel_enable(*(bl_spi_dma_tx->dma),
							  bl_spi_dma_tx->dma_channel,
							  bl_spi_dma_tx->dmamux_req,
							  tx_buf, &SPI2_DR, len/2,
							  false, DMA_TO_DEVICE);
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
	nvic_set_priority(bl_spi_dma_rx->irq, 0);
	nvic_enable_irq(bl_spi_dma_rx->irq);
	bl_acq_dma_enable(*(bl_spi_dma_rx->dma));

	nvic_set_priority(bl_spi_dma_tx->irq, 0);
	nvic_enable_irq(bl_spi_dma_tx->irq);
	bl_acq_dma_enable(*(bl_spi_dma_rx->dma));
}

/* Exported function, documented in spi.h */
void bl_spi_init(void)
{
	bl_spi__setup();
}

/* Exported function, documented in spi.h */
void bl_spi_send(uint8_t led)
{
	spi_send8(SPI2, led);
}

/* Exported function, documented in spi.h */
uint8_t bl_spi_receive(void)
{
	return spi_read8(SPI2);
}

/* Exported function, documented in spi.h */
void bl_spi_daughter_poll(void)
{
	static uint32_t gpioport, old_gpioport;
	static uint16_t gpio, old_gpio;
	static bool to_next = false;

	if (SPI_SR(SPI2) & SPI_SR_RXNE) {
		bl_spi_mode = BL_ACQ_SPI_DAUGHTER;
		uint8_t led = bl_spi_receive();
		gpioport = bl_led_get_port(led);
		gpio = bl_led_get_gpio(led);
		to_next = true;
	}
	if (to_next && gpio_get(GPIOB, GPIO12)) {
		gpio_clear(old_gpioport, old_gpio);
		gpio_set(gpioport, gpio);
		old_gpioport = gpioport;
		old_gpio = gpio;
		to_next = false;
	}
}
