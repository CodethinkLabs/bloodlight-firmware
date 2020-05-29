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

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/f3/nvic.h>

#include "error.h"
#include "util.h"
#include "acq.h"
#include "msg.h"
#include "usb.h"

#define ADC_MAX_CHANNELS 8
#define ACQ_OVERSAMPLE_MAX 8

enum acq_state {
	ACQ_STATE_IDLE,
	ACQ_STATE_CONFIGURED,
	ACQ_STATE_ACTIVE,
};

struct {
	enum acq_state state;

	uint8_t  oversample;
	uint16_t rate;
	uint16_t samples;
	uint8_t gain[BL_ACQ_PD__COUNT];
} acq_g;

enum acq_adc {
	ACQ_ADC_1,
	ACQ_ADC_2,
};

static struct adc_table {
	const uint32_t adc_addr;
	const uint32_t dma_addr;

	bool enabled;
	uint16_t src_mask;
	uint8_t sample_count;
	uint8_t channel_count;
	uint8_t channels[ADC_MAX_CHANNELS];

	volatile bool msg_ready;
	volatile uint16_t dma_buffer[ADC_MAX_CHANNELS << ACQ_OVERSAMPLE_MAX];

	uint8_t msg_channels;
	uint8_t msg_channels_max;
	union bl_msg_data msg;
	uint16_t sample_data[16];
} acq_adc_table[] = {
	[ACQ_ADC_1] = {
		.adc_addr = ADC1,
		.dma_addr = DMA1,
	},
	[ACQ_ADC_2] = {
		.adc_addr = ADC2,
		.dma_addr = DMA2,
	},
};

/**
 * Source table for ADC inputs we're interested in.
 */
static const struct source_table {
	uint8_t adc;
	uint8_t adc_channel;
	uint8_t gpio_pin;
} acq_source_table[] = {
	[BL_ACQ_PD1] = {
		.adc = ACQ_ADC_2,
		.adc_channel = 1,
		.gpio_pin = GPIO4,
	},
	[BL_ACQ_PD2] = {
		.adc = ACQ_ADC_1,
		.adc_channel = 4,
		.gpio_pin = GPIO3,
	},
	[BL_ACQ_PD3] = {
		.adc = ACQ_ADC_2,
		.adc_channel = 4,
		.gpio_pin = GPIO7,
	},
	[BL_ACQ_PD4] = {
		.adc = ACQ_ADC_2,
		.adc_channel = 2,
		.gpio_pin = GPIO5,
	},
	[BL_ACQ_3V3] = {
		.adc = ACQ_ADC_1,
		.adc_channel = 2,
		.gpio_pin = GPIO1,
	},
	[BL_ACQ_5V0] = {
		.adc = ACQ_ADC_1,
		.adc_channel = 3,
		.gpio_pin = GPIO2,
	},
};

static void __attribute__((optimize("O0"))) delay(unsigned duration)
{
	unsigned j;
	for (j = 0; j < duration; j++)
	{
		unsigned i;
		for (i = 0; i < (1 << 20); i++);
	}
}

static void bl_acq__adc_calibrate(uint32_t adc)
{
	adc_enable_regulator(adc);
	/* TODO: Replace this with delay_us(10); */
	delay(2);

	/* TODO: Explicitly calibrate single ended by setting DIFSEL. */
	adc_calibrate(adc);

	adc_disable_regulator(adc);
}

/* Exported function, documented in acq.h */
void bl_acq_init(void)
{
	rcc_periph_clock_enable(RCC_ADC12);
	rcc_periph_clock_enable(RCC_DMA1);
	rcc_periph_clock_enable(RCC_DMA2);
	rcc_periph_clock_enable(RCC_TIM1);

	/* TODO: Consider moving to setup and disabling on abort. */
	nvic_enable_irq(NVIC_DMA1_CHANNEL1_IRQ);
	nvic_enable_irq(NVIC_DMA2_CHANNEL1_IRQ);
	nvic_set_priority(NVIC_DMA1_CHANNEL1_IRQ, 1);
	nvic_set_priority(NVIC_DMA2_CHANNEL1_IRQ, 1);

	/* Note: For now all the GPIOs are on port A. */
	for (unsigned i = 0; i < BL_ARRAY_LEN(acq_source_table); i++) {
		gpio_mode_setup(GPIOA, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
				acq_source_table[i].gpio_pin);
	}

	/* Prescaler of 71 (=72) means timer runs at 1MHz. */
	timer_set_prescaler(TIM1, 71);
	/* Timer will trigger TRGO every OC1 match (once per period). */
	timer_set_oc_mode(TIM1, TIM_OC1, TIM_OCM_TOGGLE);
	timer_set_master_mode(TIM1, TIM_CR2_MMS_COMPARE_OC1REF);
	timer_enable_oc_output(TIM1, TIM_OC1);

	/* Setup ADC masters. */
	adc_set_clk_prescale(ADC1, ADC_CCR_CKMODE_DIV1);
	adc_set_clk_prescale(ADC3, ADC_CCR_CKMODE_DIV1);
	adc_set_multi_mode(ADC1, ADC_CCR_DUAL_INDEPENDENT);
	adc_set_multi_mode(ADC3, ADC_CCR_DUAL_INDEPENDENT);

	for (unsigned i = 0; i < BL_ARRAY_LEN(acq_adc_table); i++) {
		bl_acq__adc_calibrate(acq_adc_table[i].adc_addr);
	}
}

static enum bl_error bl_acq__setup_adc_table(uint16_t src_mask)
{
	bool enabled = false;

	/* Reset ADC table */
	for (unsigned i = 0; i < BL_ARRAY_LEN(acq_adc_table); i++) {
		acq_adc_table[i].enabled = false;
		acq_adc_table[i].src_mask = 0;
		acq_adc_table[i].channel_count = 0;
		acq_adc_table[i].msg_channels = 0;
	}

	/* Set up ADC table acording to sources enabled by `src_mask`. */
	for (unsigned i = 0; i < BL_ARRAY_LEN(acq_source_table); i++) {
		uint16_t src_bit = (1 << i);

		if (src_mask & src_bit) {
			uint8_t adc = acq_source_table[i].adc;
			uint8_t channel = acq_source_table[i].adc_channel;
			uint8_t channels = acq_adc_table[adc].channel_count;

			enabled = true;
			acq_adc_table[adc].enabled = true;
			acq_adc_table[adc].src_mask |= src_bit;
			acq_adc_table[adc].channels[channels] = channel;
			acq_adc_table[adc].channel_count++;
		}
	}

	if (!enabled) {
		return BL_ERROR_BAD_SOURCE_MASK;
	}

	for (unsigned i = 0; i < BL_ARRAY_LEN(acq_adc_table); i++) {
		const uint8_t max = BL_ARRAY_LEN(acq_adc_table[i].sample_data);
		const uint8_t channels = acq_adc_table[i].channel_count;

		acq_adc_table[i].msg_channels_max = (max / channels) * channels;
		acq_adc_table[i].sample_count = channels;
	}

	return BL_ERROR_NONE;
}

static void bl_acq__setup_adc(
		uint32_t adc,
		uint8_t channels[],
		uint8_t channel_count)
{
	adc_power_on(adc);
	adc_enable_dma_circular_mode(adc);
	adc_enable_dma(adc);
	adc_set_single_conversion_mode(adc);
	adc_set_sample_time_on_all_channels(adc, ADC_SMPR_SMP_601DOT5CYC);

	adc_set_regular_sequence(adc, channel_count, channels);

	/* TODO: Update libopencm3 to support this constant. */
	uint32_t extsel_tim1_trgo = 9;
	adc_enable_external_trigger_regular(adc,
			ADC_CFGR1_EXTSEL_VAL(extsel_tim1_trgo),
			ADC_CFGR1_EXTEN_BOTH_EDGES);
	adc_start_conversion_regular(adc);
}

static void bl_acq__init_sample_message(
		struct adc_table *adc_info)
{
	adc_info->msg.sample_data.type = BL_MSG_SAMPLE_DATA;
	adc_info->msg.sample_data.count = adc_info->msg_channels_max;
	adc_info->msg.sample_data.src_mask = adc_info->src_mask;
	adc_info->msg_ready = false;
}

static void bl_acq__setup_adc_dma(
		struct adc_table *adc_info)
{
	dma_channel_reset(adc_info->dma_addr, DMA_CHANNEL1);

	dma_set_peripheral_address(adc_info->dma_addr, DMA_CHANNEL1,
			(uint32_t) &ADC_DR(adc_info->adc_addr));
	dma_set_memory_address(adc_info->dma_addr, DMA_CHANNEL1,
			(uint32_t) adc_info->dma_buffer);

	dma_enable_memory_increment_mode(adc_info->dma_addr, DMA_CHANNEL1);

	dma_set_read_from_peripheral(adc_info->dma_addr, DMA_CHANNEL1);
	dma_set_peripheral_size(adc_info->dma_addr, DMA_CHANNEL1, DMA_CCR_PSIZE_16BIT);
	dma_set_memory_size(adc_info->dma_addr, DMA_CHANNEL1, DMA_CCR_MSIZE_16BIT);

	dma_enable_transfer_complete_interrupt(adc_info->dma_addr, DMA_CHANNEL1);
	dma_set_number_of_data(adc_info->dma_addr, DMA_CHANNEL1,
			adc_info->sample_count << acq_g.oversample);
	dma_enable_circular_mode(adc_info->dma_addr, DMA_CHANNEL1);
	dma_enable_channel(adc_info->dma_addr, DMA_CHANNEL1);

	bl_acq__setup_adc(
			adc_info->adc_addr,
			adc_info->channels,
			adc_info->channel_count);
}

static inline void dma_interrupt_helper(struct adc_table *adc)
{
	uint32_t sample[adc->sample_count];
	memset(sample, 0x00, adc->sample_count * sizeof(*sample));

	volatile uint16_t *p = adc->dma_buffer;
	for (unsigned i = 0; i < (1U << acq_g.oversample); i++) {
		for (unsigned j = 0; j < adc->sample_count; j++) {
			sample[j] += *p++;
		}
	}

	unsigned bits = 12 + acq_g.oversample;
	if (bits < 16) {
		unsigned shift = 16 - bits;
		for (unsigned j = 0; j < adc->sample_count; j++) {
			sample[j] = (sample[j] << shift)
					| (sample[j] >> (bits - shift));
		}
	} else if (bits > 16) {
		unsigned shift = bits - 16;
		for (unsigned j = 0; j < adc->sample_count; j++) {
			sample[j] >>= shift;
		}
	}

	for (unsigned j = 0; j < adc->sample_count; j++) {
		adc->msg.sample_data.data[adc->msg_channels + j] = sample[j];
	}

	adc->msg_channels += adc->sample_count;

	if (adc->msg_channels == adc->msg_channels_max) {
		adc->msg_ready = true;
		adc->msg_channels = 0;
	}
}

void dma1_channel1_isr(void)
{
	if (dma_get_interrupt_flag(DMA1, DMA_CHANNEL1, DMA_TCIF)) {
		dma_clear_interrupt_flags(DMA1, DMA_CHANNEL1, DMA_TCIF);
		dma_interrupt_helper(&acq_adc_table[ACQ_ADC_1]);
	}
}

void dma2_channel1_isr(void)
{
	if (dma_get_interrupt_flag(DMA2, DMA_CHANNEL1, DMA_TCIF)) {
		dma_clear_interrupt_flags(DMA2, DMA_CHANNEL1, DMA_TCIF);
		dma_interrupt_helper(&acq_adc_table[ACQ_ADC_2]);
	}
}

/* Exported function, documented in acq.h */
enum bl_error bl_acq_setup(
		uint16_t rate,
		uint16_t samples,
		uint16_t src_mask)
{
	enum bl_error error;

	if (acq_g.state == ACQ_STATE_ACTIVE) {
		return BL_ERROR_ACTIVE_ACQUISITION;
	}

	/* TODO: Add this to protocol. */
	acq_g.oversample = 4;
	acq_g.rate = rate;
	acq_g.samples = samples;
	for (unsigned i = 0; i < BL_ACQ_PD__COUNT; i++) {
		if (acq_g.gain[i] == 0) {
			acq_g.gain[i] = 1;
		}
	}

	error = bl_acq__setup_adc_table(src_mask);
	if (error != BL_ERROR_NONE) {
		return error;
	}

	for (unsigned i = 0; i < BL_ARRAY_LEN(acq_adc_table); i++) {
		if (acq_adc_table[i].enabled) {
			bl_acq__init_sample_message(&acq_adc_table[i]);
			bl_acq__setup_adc_dma(&acq_adc_table[i]);
		}
	}

	timer_set_period(TIM1, acq_g.rate);

	acq_g.state = ACQ_STATE_CONFIGURED;
	return BL_ERROR_NONE;
}

/* Exported function, documented in acq.h */
enum bl_error bl_acq_set_gains(const uint8_t gain[BL_ACQ_PD__COUNT])
{
	if (acq_g.state == ACQ_STATE_ACTIVE) {
		return BL_ERROR_ACTIVE_ACQUISITION;
	}

	for (unsigned i = 0; i < BL_ACQ_PD__COUNT; i++) {
		if ((gain[i] < 1) || (gain[i] > 16)
				|| (gain[i] & (gain[i] - 1))) {
			return BL_ERROR_OUT_OF_RANGE;
		}

		/* TODO: Set opamp gain here and select adc source. */
		acq_g.gain[i] = i;
	}

	return BL_ERROR_NONE;
}

/* Exported function, documented in acq.h */
enum bl_error bl_acq_start(void)
{
	if (acq_g.state != ACQ_STATE_CONFIGURED) {
		return BL_ERROR_ACQ_NOT_CONFIGURED;
	}

	acq_g.state = ACQ_STATE_ACTIVE;

	/* Enabling the timer will start triggering ADC. */
	timer_enable_counter(TIM1);
	return BL_ERROR_NONE;
}

/* Exported function, documented in acq.h */
enum bl_error bl_acq_abort(void)
{
	if (acq_g.state != ACQ_STATE_ACTIVE) {
		/* No-op. */
		return BL_ERROR_NONE;
	}

	/* Disabling the timer will stop any future conversions. */
	timer_disable_counter(TIM1);

	for (unsigned i = 0; i < BL_ARRAY_LEN(acq_adc_table); i++) {
		acq_adc_table[i].msg_ready = false;
		acq_adc_table[i].msg_channels = 0;
	}

	for (unsigned i = 0; i < BL_ARRAY_LEN(acq_adc_table); i++) {
		adc_power_off(acq_adc_table[i].adc_addr);
		adc_disable_dma(acq_adc_table[i].adc_addr);
		dma_disable_transfer_complete_interrupt(
				acq_adc_table[i].dma_addr, DMA_CHANNEL1);
		dma_disable_channel(acq_adc_table[i].dma_addr, DMA_CHANNEL1);
	}

	acq_g.state = ACQ_STATE_IDLE;
	return BL_ERROR_NONE;
}

/* Exported function, documented in acq.h */
void bl_acq_poll(void)
{
	for (unsigned i = 0; i < BL_ARRAY_LEN(acq_adc_table); i++) {
		if (acq_adc_table[i].msg_ready) {
			if (usb_send_message(&acq_adc_table[i].msg)) {
				acq_adc_table[i].msg_ready = false;
				break;
			}
		}
	}
}
