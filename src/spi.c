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

#include "spi.h"
#include "led.h"

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

	//spi initialization;
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
void bl_spi_init(void)
{
	bl_spi__setup();
}
