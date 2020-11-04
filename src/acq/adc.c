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

#include "adc.h"
#include "rcc.h"
#include "source.h"
#include "channel.h"

#include "common/util.h"

#include "../delay.h"
#include "../led.h"

#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/cm3/nvic.h>

#if (BL_REVISION >= 2)
#include <libopencm3/stm32/dmamux.h>
#endif


#if (BL_REVISION == 1)
	#define BL_ACQ_DMA_MAX 2048

	#define BL_ACQ_ADC_FREQ_MAX_SINGLE 72000000
	#define BL_ACQ_ADC_FREQ_MAX_MULTI  BL_ACQ_ADC_FREQ_MAX_SINGLE

	/* ADC Voltage Regulator startup time in uS. */
	#define TADCVREG_STUP 10
#else
	#define BL_ACQ_DMA_MAX 4096

	#define BL_ACQ_ADC_FREQ_MAX_SINGLE 60000000
	#define BL_ACQ_ADC_FREQ_MAX_MULTI  52000000

	/* ADC Voltage Regulator startup time in uS. */
	#define TADCVREG_STUP 20
#endif




typedef struct
{
	uint32_t ckmode;
	uint32_t presc;
	uint32_t frequency;
} bl_acq_adc_group_config_t;

struct bl_acq_adc_group_s
{
	const uint32_t rcc_unit;
	const uint32_t master;
#if (BL_REVISION == 1)
	const uint8_t  presc_shift;
#endif
	unsigned       enable;

	bl_acq_adc_group_config_t config;
};

bl_acq_adc_group_t bl_acq_adc_group12 =
{
	.rcc_unit    = RCC_ADC12,
	.master      = ADC1,
#if (BL_REVISION == 1)
	.presc_shift = RCC_CFGR2_ADC12PRES_SHIFT,
#endif
	.enable      = 0,
};

#if (BL_REVISION == 1)
bl_acq_adc_group_t bl_acq_adc_group34 =
{
	.rcc_unit    = RCC_ADC34,
	.master      = ADC3,
	.presc_shift = RCC_CFGR2_ADC34PRES_SHIFT,
	.enable      = 0,
};
#else
bl_acq_adc_group_t bl_acq_adc_group345 =
{
	.rcc_unit  = RCC_ADC345,
	.master    = ADC3,
	.enable    = 0,
};
#endif


enum bl_error bl_acq_adc_group_configure(bl_acq_adc_group_t *adc_group,
		uint32_t clock, bool async, bool multi)
{
	if (adc_group->enable > 0) {
		return BL_ERROR_ACTIVE_ACQUISITION;
	}

	uint32_t max_freq = multi ? BL_ACQ_ADC_FREQ_MAX_MULTI
		: BL_ACQ_ADC_FREQ_MAX_SINGLE;

	uint32_t ckmode;
	uint32_t presc = 0;

	uint32_t frequency = clock;
	if (async) {
		ckmode = ADC_CCR_CKMODE_CKX;

		uint16_t div_table[] = {
			1, 2, 4, 6, 8, 10, 12, 16, 32, 64, 128, 256,
		};

		while (((frequency / div_table[presc]) > max_freq) &&
				((presc + 1) < BL_ARRAY_LEN(div_table))) {
			presc++;
		}
		frequency /= div_table[presc];
	} else {
		ckmode = ADC_CCR_CKMODE_DIV1;
		if (frequency > max_freq) {
			frequency /= 2;
			ckmode = ADC_CCR_CKMODE_DIV2;
		}
		if (frequency > max_freq) {
			frequency /= 2;
			ckmode = ADC_CCR_CKMODE_DIV4;
		}
	}

	if (frequency > max_freq) {
		return BL_ERROR_ADC_FREQ_TOO_HIGH;
	}

	bl_acq_adc_group_config_t *config = &adc_group->config;

	config->ckmode    = ckmode;
	config->presc     = presc;
	config->frequency = frequency;
	return BL_ERROR_NONE;
}

enum bl_error bl_acq_adc_group_configure_all(
		uint32_t clock, bool async, bool multi)
{
	enum bl_error status;

	status = bl_acq_adc_group_configure(&bl_acq_adc_group12,
			clock, async, multi);
	if (status != BL_ERROR_NONE) {
		return status;
	}

#if (BL_REVISION == 1)
	status = bl_acq_adc_group_configure(&bl_acq_adc_group34,
			clock, async, multi);
#else
	status = bl_acq_adc_group_configure(&bl_acq_adc_group345,
			clock, async, multi);
#endif
	return status;
}

static void bl_acq_adc_group_enable(bl_acq_adc_group_t *adc_group)
{
	if (!bl_acq__rcc_enable_ref(adc_group->rcc_unit, &adc_group->enable)) {
		/* Already enabled. */
		return;
	}

	const bl_acq_adc_group_config_t *config = &adc_group->config;

#if (BL_REVISION == 1)
	uint32_t mask  = RCC_CFGR2_ADCxPRES_MASK;
	RCC_CFGR &= ~(mask << adc_group->presc_shift);

	if (config->ckmode == ADC_CCR_CKMODE_CKX) {
		uint32_t value = RCC_CFGR2_ADCxPRES_PLL_CLK_DIV_1 +
				config->presc;
		RCC_CFGR |= (value << adc_group->presc_shift);
	}

	/* This function is named incorrectly in libopencm3 for F3. */
	adc_set_clk_prescale(adc_group->master, config->ckmode);
#else
	adc_set_clk_source(adc_group->master, config->ckmode);
	if (config->ckmode == ADC_CCR_CKMODE_CKX) {
		adc_set_clk_prescale(adc_group->master, config->presc);
	}
#endif

	adc_set_multi_mode(adc_group->master, ADC_CCR_DUAL_INDEPENDENT);
}

static void bl_acq_adc_group_disable(bl_acq_adc_group_t *adc_group)
{
	bl_acq__rcc_disable_ref(adc_group->rcc_unit, &adc_group->enable);
}



#ifndef ADC_CHANNEL_COUNT
#define ADC_CHANNEL_COUNT BL_ACQ__SRC_COUNT
#endif

typedef struct
{
	unsigned channel_count;
	uint8_t  src_mask[ADC_CHANNEL_COUNT];
	uint8_t  channel_adc[ADC_CHANNEL_COUNT];
	uint8_t  channel_source[ADC_CHANNEL_COUNT];
	uint8_t  smp[ADC_CHANNEL_COUNT];

#if (BL_REVISION >= 2)
	bool    rovse;
	uint8_t ovsr;
	uint8_t ovss;
#endif
} bl_acq_adc_config_t;

struct bl_acq_adc_s
{
	bl_acq_adc_group_t *group;
	const uint32_t      base;
	bl_acq_timer_t    **timer;
	const uint32_t      extsel;
	bl_acq_dma_t      **dma;
	const uint8_t       dma_channel;
	const uint8_t       dmamux_req;
	const uint16_t      irq;
	unsigned            enable;

	bool    flash_enable;
	bool    flash_master;
	uint8_t flash_index;

	bool     calibrated;
	uint32_t calfact;

	unsigned          samples_per_dma;
	volatile uint16_t dma_buffer[BL_ACQ_DMA_MAX * 2];

	void (*isr)(bl_acq_adc_t *, volatile uint16_t *);

	bl_acq_adc_config_t config;
};


#if (BL_REVISION == 1)
#define ADC_CHANNEL_IS_FAST(x) ((x) <= 5)

#define ADC12_CFGR1_EXTSEL_TIM1_TRGO ADC_CFGR1_EXTSEL_VAL(9)
#define ADC34_CFGR1_EXTSEL_TIM1_TRGO ADC_CFGR1_EXTSEL_VAL(9)

static bl_acq_adc_t bl_acq__adc1 =
{
	.group       = &bl_acq_adc_group12,
	.base        = ADC1,
	.timer       = &bl_acq_tim1,
	.extsel      = ADC12_CFGR1_EXTSEL_TIM1_TRGO,
	.dma         = &bl_acq_dma1,
	.dma_channel = DMA_CHANNEL1,
	.irq         = NVIC_DMA1_CHANNEL1_IRQ,
	.enable      = 0,
	.calibrated  = false,
};
bl_acq_adc_t *bl_acq_adc1 = &bl_acq__adc1;

static bl_acq_adc_t bl_acq__adc2 =
{
	.group       = &bl_acq_adc_group12,
	.base        = ADC2,
	.timer       = &bl_acq_tim1,
	.extsel      = ADC12_CFGR1_EXTSEL_TIM1_TRGO,
	.dma         = &bl_acq_dma2,
	.dma_channel = DMA_CHANNEL1,
	.irq         = NVIC_DMA2_CHANNEL1_IRQ,
	.enable      = 0,
	.calibrated  = false,
};
bl_acq_adc_t *bl_acq_adc2 = &bl_acq__adc2;

static bl_acq_adc_t bl_acq__adc3 =
{
	.group       = &bl_acq_adc_group34,
	.base        = ADC3,
	.timer       = &bl_acq_tim1,
	.extsel      = ADC34_CFGR1_EXTSEL_TIM1_TRGO,
	.dma         = &bl_acq_dma2,
	.dma_channel = DMA_CHANNEL5,
	.irq         = NVIC_DMA2_CHANNEL5_IRQ,
	.enable      = 0,
	.calibrated  = false,
};
bl_acq_adc_t *bl_acq_adc3 = &bl_acq__adc3;

static bl_acq_adc_t bl_acq__adc4 =
{
	.group       = &bl_acq_adc_group34,
	.base        = ADC4,
	.timer       = &bl_acq_tim1,
	.extsel      = ADC34_CFGR1_EXTSEL_TIM1_TRGO,
	.dma         = &bl_acq_dma2,
	.dma_channel = DMA_CHANNEL2,
	.irq         = NVIC_DMA2_CHANNEL2_IRQ,
	.enable      = 0,
	.calibrated  = false,
};
bl_acq_adc_t *bl_acq_adc4 = &bl_acq__adc4;
#else
static bl_acq_adc_t bl_acq__adc1 =
{
	.group       = &bl_acq_adc_group12,
	.base        = ADC1,
	.timer       = &bl_acq_tim1,
	.extsel      = ADC12_CFGR1_EXTSEL_TIM1_TRGO,
	.dma         = &bl_acq_dma1,
	.dma_channel = DMA_CHANNEL1,
	.dmamux_req  = DMAMUX_CxCR_DMAREQ_ID_ADC1,
	.irq         = NVIC_DMA1_CHANNEL1_IRQ,
	.enable      = 0,
	.calibrated  = false,
};
bl_acq_adc_t *bl_acq_adc1 = &bl_acq__adc1;

static bl_acq_adc_t bl_acq__adc2 =
{
	.group       = &bl_acq_adc_group12,
	.base        = ADC2,
	.timer       = &bl_acq_tim1,
	.extsel      = ADC12_CFGR1_EXTSEL_TIM1_TRGO,
	.dma         = &bl_acq_dma1,
	.dma_channel = DMA_CHANNEL2,
	.dmamux_req  = DMAMUX_CxCR_DMAREQ_ID_ADC2,
	.irq         = NVIC_DMA1_CHANNEL2_IRQ,
	.enable      = 0,
	.calibrated  = false,
};
bl_acq_adc_t *bl_acq_adc2 = &bl_acq__adc2;

static bl_acq_adc_t bl_acq__adc3 =
{
	.group       = &bl_acq_adc_group345,
	.base        = ADC3,
	.timer       = &bl_acq_tim1,
	.extsel      = ADC345_CFGR1_EXTSEL_TIM1_TRGO,
	.dma         = &bl_acq_dma1,
	.dma_channel = DMA_CHANNEL3,
	.dmamux_req  = DMAMUX_CxCR_DMAREQ_ID_ADC3,
	.irq         = NVIC_DMA1_CHANNEL3_IRQ,
	.enable      = 0,
	.calibrated  = false,
};
bl_acq_adc_t *bl_acq_adc3 = &bl_acq__adc3;

static bl_acq_adc_t bl_acq__adc4 =
{
	.group       = &bl_acq_adc_group345,
	.base        = ADC4,
	.timer       = &bl_acq_tim1,
	.extsel      = ADC345_CFGR1_EXTSEL_TIM1_TRGO,
	.dma         = &bl_acq_dma1,
	.dma_channel = DMA_CHANNEL4,
	.dmamux_req  = DMAMUX_CxCR_DMAREQ_ID_ADC4,
	.irq         = NVIC_DMA1_CHANNEL4_IRQ,
	.enable      = 0,
	.calibrated  = false,
};
bl_acq_adc_t *bl_acq_adc4 = &bl_acq__adc4;

static bl_acq_adc_t bl_acq__adc5 =
{
	.group       = &bl_acq_adc_group345,
	.base        = ADC5,
	.timer       = &bl_acq_tim1,
	.extsel      = ADC345_CFGR1_EXTSEL_TIM1_TRGO,
	.dma         = &bl_acq_dma1,
	.dma_channel = DMA_CHANNEL5,
	.dmamux_req  = DMAMUX_CxCR_DMAREQ_ID_ADC5,
	.irq         = NVIC_DMA1_CHANNEL5_IRQ,
	.enable      = 0,
	.calibrated  = false,
};
bl_acq_adc_t *bl_acq_adc5 = &bl_acq__adc5;
#endif


void bl_acq_adc_calibrate(bl_acq_adc_t *adc)
{
	if (adc->enable > 0) {
		return;
	}

	bl_acq_adc_group_enable(adc->group);

#if (BL_REVISION >= 2)
	adc_disable_deeppwd(adc->base);
#endif

	adc_enable_regulator(adc->base);
	bl_delay_us(TADCVREG_STUP);

	/* Explicitly calibrate single ended. */
	ADC_CR(adc->base) &= ~ADC_CR_ADCALDIF;

	adc_calibrate(adc->base);

	adc->calibrated = true;
	adc->calfact    = ADC_CALFACT(adc->base);

	adc_disable_regulator(adc->base);

#if (BL_REVISION >= 2)
	adc_enable_deeppwd(adc->base);
#endif

	bl_acq_adc_group_disable(adc->group);
}

enum bl_error bl_acq_adc_channel_configure(bl_acq_adc_t *adc,
	uint8_t channel, uint8_t source)
{
	bl_acq_adc_config_t *config = &adc->config;

	config->channel_adc[config->channel_count] = channel;
	config->channel_source[config->channel_count] = source;
	config->src_mask[config->channel_count] = 1U << source;
	config->channel_count++;

	return BL_ERROR_NONE;
}

static void bl_acq_adc_dma_isr_single(
	bl_acq_adc_t *adc, volatile uint16_t *buffer);
static void bl_acq_adc_dma_isr_multi(
	bl_acq_adc_t *adc, volatile uint16_t *buffer);
static void bl_acq_adc_dma_isr_flash(
	bl_acq_adc_t *adc, volatile uint16_t *buffer);

enum bl_error bl_acq_adc_configure(bl_acq_adc_t *adc,
		uint32_t frequency, uint32_t sw_oversample,
		uint8_t oversample, uint8_t shift)
{
	if (sw_oversample == 0) {
		sw_oversample = 1;
	}

	/** ADC SMP constants used for frequency setting.
	 *  We subtract 0.5 from the datasheet sampling time. */
	static const struct smp_table {
		const uint32_t smp;
		const uint32_t time;
	} smp_table[] = {
#if (BL_REVISION == 1)
		{ ADC_SMPR_SMP_1DOT5CYC  ,   1 },
		{ ADC_SMPR_SMP_2DOT5CYC  ,   2 },
		{ ADC_SMPR_SMP_4DOT5CYC  ,   4 },
		{ ADC_SMPR_SMP_7DOT5CYC  ,   7 },
		{ ADC_SMPR_SMP_19DOT5CYC ,  19 },
		{ ADC_SMPR_SMP_61DOT5CYC ,  61 },
		{ ADC_SMPR_SMP_181DOT5CYC, 181 },
		{ ADC_SMPR_SMP_601DOT5CYC, 601 },
#else
		{ ADC_SMPR_SMP_2DOT5CYC  ,   2 },
		{ ADC_SMPR_SMP_6DOT5CYC  ,   6 },
		{ ADC_SMPR_SMP_12DOT5CYC ,  12 },
		{ ADC_SMPR_SMP_24DOT5CYC ,  24 },
		{ ADC_SMPR_SMP_47DOT5CYC ,  47 },
		{ ADC_SMPR_SMP_92DOT5CYC ,  92 },
		{ ADC_SMPR_SMP_247DOT5CYC, 247 },
		{ ADC_SMPR_SMP_640DOT5CYC, 640 },
#endif
	};

#if (BL_REVISION == 1)
	unsigned smp_slow_max = 2;
#else
	unsigned smp_slow_max = 1;
#endif

	bl_acq_adc_config_t *config = &adc->config;

	/* Configure ADC. */
	unsigned smp[config->channel_count];
	uint32_t smp_time = 0;
	for (unsigned i = 0; i < config->channel_count; i++) {
		smp[i] = (ADC_CHANNEL_IS_FAST(config->channel_adc[i])
				? 0 : smp_slow_max);
		smp_time += 13 + smp_table[smp[i]].time;
	}

	/* ADC Group must have been initialized/configured first. */
	uint32_t smp_max = adc->group->config.frequency / frequency;
	if (smp_time > smp_max) {
		return BL_ERROR_BAD_FREQUENCY;
	}

	if ((sw_oversample * config->channel_count) > BL_ACQ_DMA_MAX) {
		return BL_ERROR_ADC_DMA_BUFFER;
	}

#if (BL_REVISION == 1)
	if ((oversample > 0) || (shift > 0)) {
		return BL_ERROR_HARDWARE_CONFLICT;
	}
#else
	if ((oversample == 0) && (shift > 0)) {
		return BL_ERROR_HARDWARE_CONFLICT;
	}

	if (oversample > 8) {
		return BL_ERROR_OUT_OF_RANGE;
	}

	if (shift > 8) {
		return BL_ERROR_OUT_OF_RANGE;
	}

	/* TODO: Add support for SMPPLUS (3.5 cycles). */
#endif

	/* We continue to increase sampling time as long as we have headroom. */
	bool changed = true;
	while (changed) {
		changed = false;

		for (unsigned i = 0; i < config->channel_count; i++) {
			if (smp[i] >= (BL_ARRAY_LEN(smp_table) - 1)) {
				/* We're already at the max sampling period. */
				continue;
			}

			uint32_t delta = smp_table[smp[i] + 1].time -
					smp_table[smp[i]].time;
			if ((smp_time + delta) <= smp_max)
			{
				smp[i]++;
				smp_time += delta;
				changed = true;
			}
		}
	}

#if (BL_REVISION >= 2)
	config->rovse = (oversample > 0);
	config->ovsr  = (oversample - 1);
	config->ovss  = shift;
#endif

	for (unsigned i = 0; i < config->channel_count; i++) {
		config->smp[i] = smp_table[smp[i]].smp;
	}

	/* We interrupt for every collected sample to make flash patterns
	 * consistent across samples. It's possible to modify this so that
	 * we collect multiple samples per interrupt. */
	adc->samples_per_dma = sw_oversample * config->channel_count;

	/* Select optimal ISR based on configuration. */
	adc->isr = (config->channel_count > 1 ?
		bl_acq_adc_dma_isr_multi : bl_acq_adc_dma_isr_single);

	return BL_ERROR_NONE;
}

void bl_acq_adc_flash_init(bl_acq_adc_t *adc, bool enable, bool master)
{
	adc->flash_enable = enable;
	adc->flash_master = master;
	adc->flash_index  = 0;

	/* TODO: Combine this with configuration. */
	if (enable) {
		adc->isr = bl_acq_adc_dma_isr_flash;
	}
}

void bl_acq_adc_enable(bl_acq_adc_t *adc, uint32_t ccr_flag)
{
	adc->enable++;
	if (adc->enable != 1) {
		/* ADC already enabled. */
		return;
	}

	bl_acq_adc_group_enable(adc->group);

	nvic_enable_irq(adc->irq);
	nvic_set_priority(adc->irq, 1);

	bl_acq_dma_enable(*(adc->dma));
	bl_acq_dma_channel_enable(*(adc->dma), adc->dma_channel, adc->dmamux_req,
			adc->dma_buffer, &ADC_DR(adc->base),
			adc->samples_per_dma, true, DMA_FROM_DEVICE);

	const bl_acq_adc_config_t *config = &adc->config;

#if (BL_REVISION >= 2)
	adc_disable_deeppwd(adc->base);
#endif

	adc_enable_regulator(adc->base);
	bl_delay_us(TADCVREG_STUP);
	adc_power_on(adc->base);

	if (adc->calibrated) {
		ADC_CALFACT(adc->base) = adc->calfact;
	}

	adc_enable_dma_circular_mode(adc->base);
	adc_enable_dma(adc->base);
	adc_set_single_conversion_mode(adc->base);

#if (BL_REVISION >= 2)
	if (config->rovse) {
		uint32_t cfgr2 = ADC_CFGR2(adc->base);
		cfgr2 &= ~(ADC_CFGR2_JOVSE | ADC_CFGR2_OVSR_MASK |
				ADC_CFGR2_OVSS_MASK | ADC_CFGR2_TROVS |
				ADC_CFGR2_ROVSM);

		cfgr2 |= ADC_CFGR2_OVSR_VAL(config->ovsr);
		cfgr2 |= ADC_CFGR2_OVSS_VAL(config->ovss);
		cfgr2 |= ADC_CFGR2_ROVSE;

		ADC_CFGR2(adc->base) = cfgr2;
	} else {
		ADC_CFGR2(adc->base) &= ~ADC_CFGR2_ROVSE;
	}
#endif

	if (ccr_flag != 0) {
		ADC_CCR(adc->group->master) |= ccr_flag;
	}

	for (unsigned i = 0; i < config->channel_count; i++) {
		adc_set_sample_time(adc->base,
				config->channel_adc[i], config->smp[i]);
	}

	adc_set_regular_sequence(adc->base,
			config->channel_count, (uint8_t *)config->channel_adc);

	adc_enable_external_trigger_regular(adc->base, adc->extsel,
			ADC_CFGR1_EXTEN_BOTH_EDGES);
	adc_start_conversion_regular(adc->base);

	bl_acq_timer_enable(*(adc->timer));
}

void bl_acq_adc_disable(bl_acq_adc_t *adc, uint32_t ccr_flag)
{
	if (adc->enable > 1) {
		adc->enable--;
		return;
	}

	adc->enable = 0;

	bl_acq_timer_disable(*(adc->timer));

	bl_acq_dma_disable(*(adc->dma));

	nvic_disable_irq(adc->irq);

	if (ccr_flag != 0) {
		ADC_CCR(adc->group->master) &= ~ccr_flag;
	}

	adc_power_off(adc->base);
	adc_disable_regulator(adc->base);

#if (BL_REVISION >= 2)
	adc_enable_deeppwd(adc->base);
#endif

	bl_acq_adc_group_disable(adc->group);

	/* Clear configuration so we're ready for re-configuring. */
	adc->config.channel_count = 0;
}

bl_acq_adc_group_t *bl_acq_adc_get_group(const bl_acq_adc_t *adc)
{
	return adc->group;
}

bl_acq_timer_t *bl_acq_adc_get_timer(const bl_acq_adc_t *adc)
{
	return *(adc->timer);
}

bl_acq_dma_t *bl_acq_adc_get_dma(const bl_acq_adc_t *adc)
{
	return *(adc->dma);
}

static void bl_acq_adc_dma_isr_single(
	bl_acq_adc_t *adc, volatile uint16_t *buffer)
{
	uint32_t sample = 0;
	volatile uint16_t *p = buffer;
	for (unsigned s = 0; s < adc->samples_per_dma; s++) {
		sample += *p++;
	}

	unsigned channel = bl_acq_source_get_channel(
			adc->config.channel_source[0]);
	bl_acq_channel_commit_sample(channel, sample);
}

static void bl_acq_adc_dma_isr_multi(
	bl_acq_adc_t *adc, volatile uint16_t *buffer)
{
	uint32_t sample[adc->config.channel_count];
	for (unsigned c = 0; c < adc->config.channel_count; c++) {
		sample[c] = 0;
	}

	volatile uint16_t *p = buffer;
	for (unsigned s = 0; s < adc->samples_per_dma;) {
		for (unsigned c = 0; c < adc->config.channel_count; c++, s++) {
			sample[c] += *p++;
		}
	}

	for (unsigned c = 0; c < adc->config.channel_count; c++) {
		unsigned channel = bl_acq_source_get_channel(
				adc->config.channel_source[c]);
		bl_acq_channel_commit_sample(channel, sample[c]);
	}
}

static void bl_acq_adc_dma_isr_flash(
	bl_acq_adc_t *adc, volatile uint16_t *buffer)
{
	uint32_t sample[adc->config.channel_count];
	for (unsigned c = 0; c < adc->config.channel_count; c++) {
		sample[c] = 0;
	}

	volatile uint16_t *p = buffer;
	for (unsigned s = 0; s < adc->samples_per_dma;) {
		for (unsigned c = 0; c < adc->config.channel_count; c++, s++) {
			sample[c] += *p++;
		}
	}

	for (unsigned c = 0; c < adc->config.channel_count; c++) {
		if (bl_led_channel[adc->flash_index].src_mask &
			adc->config.src_mask[c]) {
			unsigned channel = bl_acq_source_get_channel(
					adc->config.channel_source[c]);
			bl_acq_channel_commit_sample(channel, sample[c]);
		}
	}

	adc->flash_index++;
	if (adc->flash_index >= bl_led_count) {
		adc->flash_index = 0;
	}

	if (adc->flash_master) {
		bl_led_loop();
	}
}

/* This is a macro to avoid repeated code for each ISR. */
#define DMA_CHANNEL_ISR(__dma, __channel, __adc) \
	void dma##__dma##_channel##__channel##_isr(void) \
	{ \
		bl_acq_adc_t *adc = bl_acq_adc##__adc; \
		if (dma_get_interrupt_flag(DMA##__dma, \
				DMA_CHANNEL##__channel, DMA_HTIF)) { \
			adc->isr(adc, &adc->dma_buffer[0]); \
			dma_clear_interrupt_flags(DMA##__dma, \
					DMA_CHANNEL##__channel, DMA_HTIF); \
		} else { \
			adc->isr(adc, &adc->dma_buffer[adc->samples_per_dma]); \
			dma_clear_interrupt_flags(DMA##__dma, \
					DMA_CHANNEL##__channel, DMA_TCIF); \
		} \
	}

#if (BL_REVISION == 1)
DMA_CHANNEL_ISR(1, 1, 1);
DMA_CHANNEL_ISR(2, 1, 2);
DMA_CHANNEL_ISR(2, 5, 3);
DMA_CHANNEL_ISR(2, 2, 4);
#else
DMA_CHANNEL_ISR(1, 1, 1);
DMA_CHANNEL_ISR(1, 2, 2);
DMA_CHANNEL_ISR(1, 3, 3);
DMA_CHANNEL_ISR(1, 4, 4);
DMA_CHANNEL_ISR(1, 5, 5);
#endif

