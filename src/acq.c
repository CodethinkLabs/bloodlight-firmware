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
#include <libopencm3/stm32/opamp.h>
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
	uint8_t gain[BL_ACQ_PD__COUNT];
	uint8_t opamp[BL_ACQ_PD__COUNT];
} acq_g;

enum acq_adc {
	ACQ_ADC_1,
	ACQ_ADC_2,
	ACQ_ADC_3,
	ACQ_ADC_4,
};

enum acq_opamp {
	ACQ_OPAMP_1,
	ACQ_OPAMP_2,
	ACQ_OPAMP_3,
	ACQ_OPAMP_4,
};

static struct adc_table {
	const uint32_t adc_addr;
	const uint32_t dma_addr;
	const uint32_t dma_chan;

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
		.dma_chan = DMA_CHANNEL1,
	},
	[ACQ_ADC_2] = {
		.adc_addr = ADC2,
		.dma_addr = DMA2,
		.dma_chan = DMA_CHANNEL1,
	},
	[ACQ_ADC_3] = {
		.adc_addr = ADC3,
		.dma_addr = DMA2,
		.dma_chan = DMA_CHANNEL5,
	},
	[ACQ_ADC_4] = {
		.adc_addr = ADC4,
		.dma_addr = DMA2,
		.dma_chan = DMA_CHANNEL2,
	},
};

/**
 * Source table for ADC inputs we're interested in.
 */
static const struct source_table {
	uint8_t adc;
	uint8_t adc_channel;
	uint8_t gpio_pin;
	uint8_t opamp_mask;
} acq_source_table[] = {
	[BL_ACQ_PD1] = {
		.adc = ACQ_ADC_2,
		.adc_channel = 1,
		.gpio_pin = GPIO4,
		.opamp_mask = 0x8,
	},
	[BL_ACQ_PD2] = {
		.adc = ACQ_ADC_1,
		.adc_channel = 4,
		.gpio_pin = GPIO3,
		.opamp_mask = 0x1,
	},
	[BL_ACQ_PD3] = {
		.adc = ACQ_ADC_2,
		.adc_channel = 4,
		.gpio_pin = GPIO7,
		.opamp_mask = 0x3,
	},
	[BL_ACQ_PD4] = {
		.adc = ACQ_ADC_2,
		.adc_channel = 2,
		.gpio_pin = GPIO5,
		.opamp_mask = 0x5,
	},
	[BL_ACQ_3V3] = {
		.adc = ACQ_ADC_1,
		.adc_channel = 2,
		.gpio_pin = GPIO1,
		.opamp_mask = 0x0,
	},
	[BL_ACQ_5V0] = {
		.adc = ACQ_ADC_1,
		.adc_channel = 3,
		.gpio_pin = GPIO2,
		.opamp_mask = 0x5,
	},
};

/**
 * Source table for ADC OPAMP inputs we're interested in.
 */
static const struct opamp_source_table {
	uint32_t opamp_addr;
	uint8_t  adc;
	uint8_t  adc_channel;
} acq_opamp_source_table[] = {
	[ACQ_OPAMP_1] = {
		.opamp_addr = OPAMP1,
		.adc = ACQ_ADC_1,
		.adc_channel = 3,
	},
	[ACQ_OPAMP_2] = {
		.opamp_addr = OPAMP2,
		.adc = ACQ_ADC_2,
		.adc_channel = 3,
	},
	[ACQ_OPAMP_3] = {
		.opamp_addr = OPAMP3,
		.adc = ACQ_ADC_3,
		.adc_channel = 1,
	},
	[ACQ_OPAMP_4] = {
		.opamp_addr = OPAMP4,
		.adc = ACQ_ADC_4,
		.adc_channel = 3,
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

static void bl_acq__opamp_calibrate(uint32_t opamp)
{
	(void)opamp;
	/* TODO: Implement OPAMP trimming. */
}

/* Exported function, documented in acq.h */
void bl_acq_init(void)
{
	rcc_periph_clock_enable(RCC_ADC12);
	rcc_periph_clock_enable(RCC_ADC34);
	/* TODO: Find out whether SYSCFG needs enabling for the opamps. */
	rcc_periph_clock_enable(RCC_SYSCFG);
	rcc_periph_clock_enable(RCC_DMA1);
	rcc_periph_clock_enable(RCC_DMA2);
	rcc_periph_clock_enable(RCC_TIM1);

	/* TODO: Consider moving to setup and disabling on abort. */
	nvic_enable_irq(NVIC_DMA1_CHANNEL1_IRQ);
	nvic_enable_irq(NVIC_DMA2_CHANNEL1_IRQ);
	nvic_enable_irq(NVIC_DMA2_CHANNEL5_IRQ);
	nvic_enable_irq(NVIC_DMA2_CHANNEL2_IRQ);
	nvic_set_priority(NVIC_DMA1_CHANNEL1_IRQ, 1);
	nvic_set_priority(NVIC_DMA2_CHANNEL1_IRQ, 1);
	nvic_set_priority(NVIC_DMA2_CHANNEL5_IRQ, 1);
	nvic_set_priority(NVIC_DMA2_CHANNEL2_IRQ, 1);

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
	/* TODO: Fix these constants in libopencm3 so they're ADC12 and ADC34 */
	adc_set_clk_prescale(ADC1, ADC_CCR_CKMODE_DIV1);
	adc_set_clk_prescale(ADC3, ADC_CCR_CKMODE_DIV1);
	adc_set_multi_mode(ADC1, ADC_CCR_DUAL_INDEPENDENT);
	adc_set_multi_mode(ADC3, ADC_CCR_DUAL_INDEPENDENT);

	for (unsigned i = 0; i < BL_ARRAY_LEN(acq_adc_table); i++) {
		bl_acq__adc_calibrate(acq_adc_table[i].adc_addr);
	}

	for (unsigned i = 0; i < BL_ARRAY_LEN(acq_opamp_source_table); i++) {
		bl_acq__opamp_calibrate(acq_opamp_source_table[i].opamp_addr);
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

			if ((i < BL_ARRAY_LEN(acq_g.gain))
					&& (acq_g.gain[i] > 1)) {
				adc = acq_opamp_source_table[
					acq_g.opamp[i]].adc;
				channel = acq_opamp_source_table[
					acq_g.opamp[i]].adc_channel;
			}

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

static void bl_acq__setup_adc_dma(struct adc_table *adc_info)
{
	uint32_t addr = adc_info->dma_addr;
	uint32_t chan = adc_info->dma_chan;

	dma_channel_reset(addr, chan);

	dma_set_peripheral_address(addr, chan,
			(uint32_t) &ADC_DR(adc_info->adc_addr));
	dma_set_memory_address(addr, chan, (uint32_t) adc_info->dma_buffer);

	dma_enable_memory_increment_mode(addr, chan);

	dma_set_read_from_peripheral(addr, chan);
	dma_set_peripheral_size(addr, chan, DMA_CCR_PSIZE_16BIT);
	dma_set_memory_size(addr, chan, DMA_CCR_MSIZE_16BIT);

	dma_enable_transfer_complete_interrupt(addr, chan);
	dma_set_number_of_data(addr, chan,
			adc_info->sample_count << acq_g.oversample);
	dma_enable_circular_mode(addr, chan);
	dma_enable_channel(addr, chan);

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

void dma2_channel5_isr(void)
{
	if (dma_get_interrupt_flag(DMA2, DMA_CHANNEL5, DMA_TCIF)) {
		dma_clear_interrupt_flags(DMA2, DMA_CHANNEL5, DMA_TCIF);
		dma_interrupt_helper(&acq_adc_table[ACQ_ADC_3]);
	}
}

void dma2_channel2_isr(void)
{
	if (dma_get_interrupt_flag(DMA2, DMA_CHANNEL2, DMA_TCIF)) {
		dma_clear_interrupt_flags(DMA2, DMA_CHANNEL2, DMA_TCIF);
		dma_interrupt_helper(&acq_adc_table[ACQ_ADC_4]);
	}
}

/**
 * Set up the photodiode gains.
 *
 * \param[in]  gain  Gain value for each photodiode.
 * \return \ref BL_ERROR_NONE on success, or appropriate error otherwise.
 */
static enum bl_error bl_acq_set_gains(const uint8_t gain[BL_ACQ_PD__COUNT])
{
	uint8_t opamp_avail_mask = 0xf;
	uint8_t opamp_alloc_mask= 0x0;

	/* We can't use OPAMP1 unless 3V3 divider is desoldered. */
	opamp_avail_mask &= ~(1U << ACQ_OPAMP_1);

	/* We can't use OPAMP3 because it clashes with LED1. */
	opamp_avail_mask &= ~(1U << ACQ_OPAMP_3);

	uint8_t  opamp_alloc[BL_ACQ_PD__COUNT];
	uint8_t  opamp_gain[BL_ACQ_OPAMP__COUNT];
	uint32_t opamp_vpsel[BL_ACQ_OPAMP__COUNT];

	static const uint32_t vpsel_matrix
			[BL_ACQ_OPAMP__COUNT][BL_ACQ_PD__COUNT] = {
		[ACQ_OPAMP_1] = {
			0,
			OPAMP1_CSR_VP_SEL_PA3,
			OPAMP1_CSR_VP_SEL_PA7,
			OPAMP1_CSR_VP_SEL_PA5 },
		[ACQ_OPAMP_2] = {
			0,
			0,
			OPAMP2_CSR_VP_SEL_PA7,
			0 },
		[ACQ_OPAMP_3] = {
			0,
			0,
			0,
			OPAMP3_CSR_VP_SEL_PA5 },
		[ACQ_OPAMP_4] = {
			OPAMP4_CSR_VP_SEL_PA4,
			0,
			0,
			0 },
	};

	for (unsigned i = 0; i < BL_ACQ_PD__COUNT; i++) {
		if ((gain[i] < 1) || (gain[i] > 16)
				|| (gain[i] & (gain[i] - 1))) {
			return BL_ERROR_OUT_OF_RANGE;
		}

		opamp_alloc[i] = 0;
		if (gain[i] > 1) {
			uint8_t opamp_mask = acq_source_table[i].opamp_mask;
			opamp_mask &= opamp_avail_mask;

			if (opamp_mask == 0) {
				return BL_ERROR_OUT_OF_RANGE;
			}

			/* Pick the first available opamp. */
			for (unsigned j = 0; j < BL_ACQ_OPAMP__COUNT; j++) {
				if ((opamp_mask & (1U << j)) == 0) {
					continue;
				}

				opamp_alloc[i] = j;
				opamp_gain[j]  = gain[i];
				opamp_vpsel[j] = vpsel_matrix[j][i];
				break;
			}
			opamp_avail_mask &= ~(1U << opamp_alloc[i]);
			opamp_alloc_mask |=  (1U << opamp_alloc[i]);
		}
	}

	for (unsigned i = 0; i < BL_ACQ_OPAMP__COUNT; i++) {
		uint32_t opamp = acq_opamp_source_table[i].opamp_addr;

		if ((opamp_alloc_mask & (1U << i)) == 0) {
			opamp_disable(opamp);
			continue;
		}

		opamp_enable(opamp);

		uint32_t raw_gain;
		switch (opamp_gain[i]) {
		case 2:
			raw_gain = OPAMP_CSR_PGA_GAIN_2;
			break;

		case 4:
			raw_gain = OPAMP_CSR_PGA_GAIN_4;
			break;

		case 8:
			raw_gain = OPAMP_CSR_PGA_GAIN_8;
			break;

		default:
			raw_gain = OPAMP_CSR_PGA_GAIN_16;
			break;
		}

		opamp_pga_gain_select(opamp, raw_gain);

		opamp_vm_select(opamp, OPAMP_CSR_VM_SEL_PGA_MODE);
		opamp_vp_select(opamp, opamp_vpsel[i]);
	}

	for (unsigned i = 0; i < BL_ACQ_PD__COUNT; i++) {
		acq_g.gain[i]  = gain[i];
		acq_g.opamp[i] = opamp_alloc[i];
	}

	return BL_ERROR_NONE;
}

/* Exported function, documented in acq.h */
enum bl_error bl_acq_setup(
		uint16_t rate,
		uint8_t  oversample,
		uint16_t src_mask,
		const uint8_t gain[BL_ACQ_PD__COUNT])
{
	enum bl_error error;

	if (oversample > ACQ_OVERSAMPLE_MAX) {
		return BL_ERROR_OUT_OF_RANGE;
	}

	if (acq_g.state == ACQ_STATE_ACTIVE) {
		return BL_ERROR_ACTIVE_ACQUISITION;
	}

	acq_g.oversample = oversample;
	acq_g.rate = rate;
	for (unsigned i = 0; i < BL_ACQ_PD__COUNT; i++) {
		if (acq_g.gain[i] == 0) {
			acq_g.gain[i] = 1;
		}
	}

	error = bl_acq_set_gains(gain);
	if (error != BL_ERROR_NONE) {
		return error;
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
				acq_adc_table[i].dma_addr,
				acq_adc_table[i].dma_chan);
		dma_disable_channel(
				acq_adc_table[i].dma_addr,
				acq_adc_table[i].dma_chan);
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
