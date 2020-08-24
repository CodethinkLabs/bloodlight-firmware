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

#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/opamp.h>
#include <libopencm3/stm32/timer.h>

#if (BL_REVISION >= 2)
#include <libopencm3/stm32/dac.h>
#include <libopencm3/stm32/dmamux.h>
#endif

#include "delay.h"
#include "error.h"
#include "util.h"
#include "acq.h"
#include "msg.h"
#include "mq.h"
#include "usb.h"


#if (BL_REVISION == 1)
	#define BL_ACQ_DMA_MAX 2048

	#define BL_ACQ_ADC_FREQ_MAX_SINGLE 72000000
	#define BL_ACQ_ADC_FREQ_MAX_MULTI  BL_ACQ_ADC_FREQ_MAX_SINGLE

	/* ADC Voltage Regulator startup time in uS. */
	#define TADCVREG_STUP 10

	/* ADC calibration max trimming time in uS. */
	#define TOFFTRIM_MAX 2000
#else
	#define BL_ACQ_DMA_MAX 4096

	#define BL_ACQ_ADC_FREQ_MAX_SINGLE 60000000
	#define BL_ACQ_ADC_FREQ_MAX_MULTI  52000000

	/* ADC Voltage Regulator startup time in uS. */
	#define TADCVREG_STUP 20

	/* ADC calibration max trimming time in uS. */
	#define TOFFTRIM_MAX 1000
#endif

static uint32_t bl_acq__ahb_freq = 0;
static uint32_t bl_acq__adc_freq = 0;

static bool bl_acq_is_active = false;


static bool bl_acq__rcc_enable_ref(uint32_t rcc_unit, unsigned *ref)
{
	*ref += 1;

	if (*ref == 1) {
		rcc_periph_clock_enable(rcc_unit);
		return true;
	}

	return false;
}

static bool bl_acq__rcc_disable_ref(uint32_t rcc_unit, unsigned *ref)
{
	if (*ref == 0) {
		return false;
	}

	*ref -= 1;
	if (*ref == 0) {
		rcc_periph_clock_disable(rcc_unit);
		return true;
	}

	return false;
}



typedef struct
{
	uint32_t prescale;
	uint32_t period;
} bl_acq_timer_config_t;

typedef struct
{
	const uint32_t rcc_unit;
	const uint32_t base;
	uint32_t       frequency;
	unsigned       enable;

	bl_acq_timer_config_t config;
} bl_acq_timer_t;

static bl_acq_timer_t bl_acq_tim1 =
{
	.rcc_unit  = RCC_TIM1,
	.base      = TIM1,
	.enable    = 0,
};

static bl_acq_timer_t *bl_acq_timer[] =
{
	&bl_acq_tim1,
};


#define DIV_NEAREST(_num, _den) (((_num) + (_den) / 2) / (_den))

static enum bl_error bl_acq_timer_configure(bl_acq_timer_t *timer,
		uint32_t frequency)
{
	if (timer->enable > 0) {
		return BL_ERROR_ACTIVE_ACQUISITION;
	}

	if ((frequency == 0) || (frequency > bl_acq__ahb_freq)) {
		return BL_ERROR_OUT_OF_RANGE;
	}

	uint32_t ticks_per_sample = DIV_NEAREST(bl_acq__ahb_freq, frequency);

	/* Find prime factors and increase prescale. */
	uint32_t prescale = ((ticks_per_sample & 1) ? 1 : 2);
	uint32_t ticks   = ticks_per_sample / prescale;
	for (unsigned i = 3; (i * i) < ticks; i += 2) {
		if ((ticks % i) == 0) {
			if ((prescale * i) > 65536) {
				break;
			}

			ticks    /= i;
			prescale *= i;
		}
	}

	/* It's very unlikely that ticks_per_sample is a prime above 65536,
	 * however we handle that via division here. */
	while (ticks > 65536) {
		ticks = DIV_NEAREST(ticks, 2);
		prescale *= 2;
	}

	timer->config.prescale = prescale;
	timer->config.period   = ticks;
	return BL_ERROR_NONE;
}

static void bl_acq_timer_enable(bl_acq_timer_t *timer)
{
	if (!bl_acq__rcc_enable_ref(timer->rcc_unit, &timer->enable)) {
		/* Timer was already enabled. */
		return;
	}

	timer_set_prescaler(timer->base, (timer->config.prescale - 1));
	timer_set_period(timer->base, (timer->config.period - 1));

	timer->frequency = bl_acq__ahb_freq / (timer->config.prescale *
			timer->config.period);

	/* TODO: Enable specific triggers rather than just OC1 */

	/* Timer will trigger TRGO every OC1 match (once per period). */
	timer_set_oc_mode(timer->base, TIM_OC1, TIM_OCM_TOGGLE);
	timer_set_master_mode(timer->base, TIM_CR2_MMS_COMPARE_OC1REF);
	timer_enable_oc_output(timer->base, TIM_OC1);
}

static void bl_acq_timer_disable(bl_acq_timer_t *timer)
{
	bl_acq__rcc_disable_ref(timer->rcc_unit, &timer->enable);
}

static void bl_acq_timer_start(bl_acq_timer_t *timer)
{
	timer_enable_counter(timer->base);
}

static void bl_acq_timer_stop(bl_acq_timer_t *timer)
{
	timer_disable_counter(timer->base);
}

static void bl_acq_timer_start_all(void)
{
	for (unsigned i = 0; i < BL_ARRAY_LEN(bl_acq_timer); i++) {
		bl_acq_timer_t *timer = bl_acq_timer[i];
		if (timer->enable > 0) {
			bl_acq_timer_start(timer);
		}
	}
}

static void bl_acq_timer_stop_all(void)
{
	for (unsigned i = 0; i < BL_ARRAY_LEN(bl_acq_timer); i++) {
		bl_acq_timer_t *timer = bl_acq_timer[i];
		if (timer->enable > 0) {
			bl_acq_timer_stop(timer);
		}
	}
}



typedef struct
{
	const uint32_t rcc_unit;
	const uint32_t base;

#if (BL_REVISION >= 2)
	const uint32_t dmamux;
	const uint32_t dmamux_offset;
	const uint32_t dmamux_rcc;
#endif

	bool           enable;
	uint8_t        channel_mask;
} bl_acq_dma_t;

static bl_acq_dma_t bl_acq_dma1 =
{
	.rcc_unit      = RCC_DMA1,
	.base          = DMA1,
#if (BL_REVISION >= 2)
	.dmamux        = DMAMUX1,
	.dmamux_offset = 0,
	.dmamux_rcc    = RCC_DMAMUX1,
#endif
	.enable        = 0,
	.channel_mask  = 0x00,
};

#if (BL_REVISION == 1)
static bl_acq_dma_t bl_acq_dma2 =
{
	.rcc_unit      = RCC_DMA2,
	.base          = DMA2,
	.enable        = 0,
	.channel_mask  = 0x00,
};
#endif

static void bl_acq_dma_enable(bl_acq_dma_t *dma)
{
	if (dma->enable) {
		/* Already enabled. */
		return;
	}

	rcc_periph_clock_enable(dma->rcc_unit);

#if (BL_REVISION >= 2)
	rcc_periph_clock_enable(dma->dmamux_rcc);
#endif
}

static void bl_acq_dma_channel_enable(bl_acq_dma_t *dma,
		unsigned channel, uint8_t dmareq,
		volatile void *dst, volatile void *src, uint32_t count)
{
#if (BL_REVISION >= 2)
	dmareq += dma->dmamux_offset;

	dmamux_set_dma_channel_request(dma->dmamux, channel, dmareq);
#else
	(void)dmareq;
#endif

	dma_channel_reset(dma->base, channel);

	dma_set_peripheral_address(dma->base, channel, (uint32_t)src);
	dma_set_memory_address(dma->base, channel, (uint32_t)dst);

	dma_enable_memory_increment_mode(dma->base, channel);

	dma_set_read_from_peripheral(dma->base, channel);
	dma_set_peripheral_size(dma->base, channel, DMA_CCR_PSIZE_16BIT);
	dma_set_memory_size(dma->base, channel, DMA_CCR_MSIZE_16BIT);

	dma_enable_half_transfer_interrupt(dma->base, channel);
	dma_enable_transfer_complete_interrupt(dma->base, channel);

	dma_set_number_of_data(dma->base, channel, (count * 2));

	dma_enable_circular_mode(dma->base, channel);
	dma_enable_channel(dma->base, channel);

	dma->channel_mask |= 1U << channel;
}

static void bl_acq_dma_disable(bl_acq_dma_t *dma)
{
	/* Disable all channels. */
	for (unsigned i = 0; i < (sizeof(dma->channel_mask) * 8); i++) {
		if ((dma->channel_mask & (1U << i)) == 0) {
			continue;
		}

		dma_disable_transfer_complete_interrupt(dma->base, i);
		dma_disable_half_transfer_interrupt(dma->base, i);
		dma_disable_channel(dma->base, i);
	}
	dma->channel_mask = 0x00;

	/* Don't disable DMA or DMAMUX because they may be used elsewhere. */
}



typedef struct
{
	uint32_t ckmode;
	uint32_t presc;
	uint32_t frequency;
} bl_acq_adc_group_config_t;

typedef struct
{
	const uint32_t rcc_unit;
	const uint32_t master;
#if (BL_REVISION == 1)
	const uint8_t  presc_shift;
#endif
	unsigned       enable;

	bl_acq_adc_group_config_t config;
} bl_acq_adc_group_t;

static bl_acq_adc_group_t bl_acq_adc_group12 =
{
	.rcc_unit    = RCC_ADC12,
	.master      = ADC1,
#if (BL_REVISION == 1)
	.presc_shift = RCC_CFGR2_ADC12PRES_SHIFT,
#endif
	.enable      = 0,
};

#if (BL_REVISION == 1)
static bl_acq_adc_group_t bl_acq_adc_group34 =
{
	.rcc_unit    = RCC_ADC34,
	.master      = ADC3,
	.presc_shift = RCC_CFGR2_ADC34PRES_SHIFT,
	.enable      = 0,
};
#else
static bl_acq_adc_group_t bl_acq_adc_group345 =
{
	.rcc_unit  = RCC_ADC345,
	.master    = ADC3,
	.enable    = 0,
};
#endif


static enum bl_error bl_acq_adc_group_configure(bl_acq_adc_group_t *adc_group,
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
		return BL_ERROR_OUT_OF_RANGE;
	}

	bl_acq_adc_group_config_t *config = &adc_group->config;

	config->ckmode    = ckmode;
	config->presc     = presc;
	config->frequency = frequency;
	return BL_ERROR_NONE;
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
#define ADC_CHANNEL_COUNT 19
#endif

typedef struct
{
	unsigned channel_count;
	uint8_t  channel_adc[ADC_CHANNEL_COUNT];
	uint8_t  channel_index[ADC_CHANNEL_COUNT];
	uint8_t  smp[ADC_CHANNEL_COUNT];
} bl_acq_adc_config_t;

typedef struct
{
	bl_acq_adc_group_t *group;
	const uint32_t      base;
	bl_acq_timer_t     *timer;
	const uint32_t      extsel;
	bl_acq_dma_t       *dma;
	const uint8_t       dma_channel;
	const uint8_t       dmamux_req;
	const uint16_t      irq;
	unsigned            enable;

	bool     calibrated;
	uint32_t calfact;

	unsigned          samples_per_dma;
	volatile uint16_t dma_buffer[BL_ACQ_DMA_MAX * 2];

	bl_acq_adc_config_t config;
} bl_acq_adc_t;

#if (BL_REVISION == 1)
#define ADC_CHANNEL_IS_FAST(x) ((x) <= 5)

#define ADC12_CFGR1_EXTSEL_TIM1_TRGO ADC_CFGR1_EXTSEL_VAL(9)
#define ADC34_CFGR1_EXTSEL_TIM1_TRGO ADC_CFGR1_EXTSEL_VAL(9)

static bl_acq_adc_t bl_acq_adc1 =
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

static bl_acq_adc_t bl_acq_adc2 =
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

static bl_acq_adc_t bl_acq_adc3 =
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

static bl_acq_adc_t bl_acq_adc4 =
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
#else
static bl_acq_adc_t bl_acq_adc1 =
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

static bl_acq_adc_t bl_acq_adc2 =
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

static bl_acq_adc_t bl_acq_adc3 =
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

static bl_acq_adc_t bl_acq_adc4 =
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

static bl_acq_adc_t bl_acq_adc5 =
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
#endif


static void bl_acq_adc_calibrate(bl_acq_adc_t *adc)
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

static enum bl_error bl_acq_adc_channel_configure(bl_acq_adc_t *adc,
	uint8_t channel, uint8_t index)
{
	bl_acq_adc_config_t *config = &adc->config;

	config->channel_adc[config->channel_count] = channel;
	config->channel_index[config->channel_count] = index;
	config->channel_count++;

	return BL_ERROR_NONE;
}

static enum bl_error bl_acq_adc_configure(bl_acq_adc_t *adc,
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
		return BL_ERROR_OUT_OF_RANGE;
	}

#if (BL_REVISION == 1)
	if ((oversample > 0) || (shift > 0)) {
		return BL_ERROR_HARDWARE_CONFLICT;
	}
#else
	if (oversample > 0) {
		uint32_t cfgr2 = ADC_CFGR2(adc->base);
		cfgr2 &= ~(ADC_CFGR2_JOVSE | ADC_CFGR2_OVSR_MASK |
				ADC_CFGR2_OVSS_MASK | ADC_CFGR2_TROVS |
				ADC_CFGR2_ROVSM);

		cfgr2 |= ADC_CFGR2_OVSR_VAL(oversample - 1);
		cfgr2 |= ADC_CFGR2_OVSS_VAL(shift);
		cfgr2 |= ADC_CFGR2_ROVSE;

		ADC_CFGR2(adc->base) = cfgr2;
	} else if (shift > 0) {
		return BL_ERROR_HARDWARE_CONFLICT;
	} else {
		ADC_CFGR2(adc->base) &= ~ADC_CFGR2_ROVSE;
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

	for (unsigned i = 0; i < config->channel_count; i++) {
		config->smp[i] = smp_table[smp[i]].smp;
	}

	/* We interrupt for every collected sample to make flash patterns
	 * consistent across samples. It's possible to modify this so that
	 * we collect multiple samples per interrupt. */
	adc->samples_per_dma = sw_oversample * config->channel_count;

	return BL_ERROR_NONE;
}

static void bl_acq_adc_enable(bl_acq_adc_t *adc, uint32_t ccr_flag)
{
	adc->enable++;
	if (adc->enable != 1) {
		/* ADC already enabled. */
		return;
	}

	bl_acq_adc_group_enable(adc->group);

	nvic_enable_irq(adc->irq);
	nvic_set_priority(adc->irq, 1);

	bl_acq_dma_enable(adc->dma);
	bl_acq_dma_channel_enable(adc->dma, adc->dma_channel, adc->dmamux_req,
			adc->dma_buffer, &ADC_DR(adc->base),
			adc->samples_per_dma);

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

	bl_acq_timer_enable(adc->timer);
}

static void bl_acq_adc_disable(bl_acq_adc_t *adc, uint32_t ccr_flag)
{
	if (adc->enable > 1) {
		adc->enable--;
		return;
	}

	adc->enable = 0;

	bl_acq_timer_disable(adc->timer);

	bl_acq_dma_disable(adc->dma);

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



#if (BL_REVISION >= 2)
typedef struct
{
	uint8_t  channel_mask;
	uint16_t offset[2];
} bl_acq_dac_config_t;

typedef struct
{
	const uint32_t rcc_unit;
	const uint32_t base;
	unsigned       enable;

	bl_acq_dac_config_t config;
} bl_acq_dac_t;

static bl_acq_dac_t bl_acq_dac3 =
{
	.rcc_unit = RCC_DAC3,
	.base     = DAC3,
	.enable   = 0,
};

static bl_acq_dac_t bl_acq_dac4 =
{
	.rcc_unit = RCC_DAC4,
	.base     = DAC4,
	.enable   = 0,
};


static void bl_acq_dac_calibrate(bl_acq_dac_t *dac)
{
	if (dac->enable) return;

	/* TODO: Implement by reading Section 22.4.13 of RM04440. */
}

static enum bl_error bl_acq_dac_channel_configure(bl_acq_dac_t *dac,
		uint8_t channel, uint16_t offset)
{
	if (dac->enable > 0) {
		return BL_ERROR_ACTIVE_ACQUISITION;
	}

	if (channel > DAC_CHANNEL2) {
		return BL_ERROR_OUT_OF_RANGE;
	}

	if (offset > 0xFFF) {
		return BL_ERROR_OUT_OF_RANGE;
	}

	bl_acq_dac_config_t *config = &dac->config;

	config->channel_mask |= channel;
	config->offset[channel] = offset;
	return BL_ERROR_NONE;
}

static void bl_acq_dac_enable(bl_acq_dac_t *dac)
{
	if (!bl_acq__rcc_enable_ref(dac->rcc_unit, &dac->enable)) {
		/* DAC already enabled. */
		return;
	}

	const bl_acq_dac_config_t *config = &dac->config;

	uint32_t hfsel = DAC_MCR_HFSEL_DIS;
	if (bl_acq__ahb_freq >= 160000000) {
		hfsel = DAC_MCR_HFSEL_AHB160;
	} else if (bl_acq__ahb_freq >= 80000000) {
		hfsel = DAC_MCR_HFSEL_AHB80;
	}
	dac_set_high_frequency_mode(dac->base, hfsel);

	uint32_t mode = 0;
	if (config->channel_mask & DAC_CHANNEL1) {
		mode |= DAC_MCR_MODE1_EP;
	}
	if (config->channel_mask & DAC_CHANNEL2) {
		mode |= DAC_MCR_MODE2_EP;
	}
	dac_set_mode(dac->base, mode);

	dac_enable(dac->base, dac->config.channel_mask);
	dac_wait_on_ready(dac->base, dac->config.channel_mask);

	if (config->channel_mask & DAC_CHANNEL1) {
		dac_load_data_buffer_single(dac->base,
				config->offset[DAC_CHANNEL1],
				DAC_RIGHT12, DAC_CHANNEL1);
	}
	if (config->channel_mask & DAC_CHANNEL2) {
		dac_load_data_buffer_single(dac->base,
				config->offset[DAC_CHANNEL2],
				DAC_RIGHT12, DAC_CHANNEL2);
	}
}

static void bl_acq_dac_disable(bl_acq_dac_t *dac)
{
	if (bl_acq__rcc_disable_ref(dac->rcc_unit, &dac->enable)) {
		dac_disable(dac->base, DAC_CHANNEL_BOTH);
	}
}
#endif



typedef struct
{
	uint32_t pga_gain;
} bl_acq_opamp_config_t;

typedef struct
{
	const uint32_t base;
#if (BL_REVISION >= 2)
	bl_acq_dac_t  *dac;
	const uint8_t  dac_channel;
#endif
	bl_acq_adc_t  *adc;
	const uint8_t  adc_channel;
	uint32_t       enable;

	uint8_t vinp;
	bool    inverted;

	bool    calibrated;
	uint8_t trimoffsetn;
	uint8_t trimoffsetp;

	bl_acq_opamp_config_t config;
} bl_acq_opamp_t;

#if (BL_REVISION == 1)
static bl_acq_opamp_t bl_acq_opamp2 =
{
	.base        = OPAMP2,
	.adc         = &bl_acq_adc2,
	.adc_channel = 3,
	.enable      = 0,
	.vinp        = OPAMP2_CSR_VP_SEL_PA7,
	.inverted    = false,
	.calibrated  = false,
};

static bl_acq_opamp_t bl_acq_opamp4 =
{
	.base        = OPAMP4,
	.adc         = &bl_acq_adc4,
	.adc_channel = 3,
	.enable      = 0,
	.vinp        = OPAMP4_CSR_VP_SEL_PA4,
	.inverted    = false,
	.calibrated  = false,
};
#else
static bl_acq_opamp_t bl_acq_opamp1 =
{
	.base        = OPAMP1,
	.dac         = &bl_acq_dac3,
	.dac_channel = DAC_CHANNEL1,
	.adc         = &bl_acq_adc1,
	.adc_channel = 13,
	.enable      = 0,
	.vinp        = OPAMP1_CSR_VP_SEL_DAC3CH1,
	.inverted    = true,
	.calibrated  = false,
};

static bl_acq_opamp_t bl_acq_opamp2 =
{
	.base        = OPAMP2,
	.dac         = NULL,
	.adc         = &bl_acq_adc2,
	.adc_channel = 16,
	.enable      = 0,
	.vinp        = OPAMP_CSR_VP_SEL_VINP0,
	.inverted    = false,
	.calibrated  = false,
};

static bl_acq_opamp_t bl_acq_opamp3 =
{
	.base        = OPAMP3,
	.dac         = &bl_acq_dac3,
	.dac_channel = DAC_CHANNEL2,
	.adc         = &bl_acq_adc3,
	.adc_channel = 13,
	.enable      = 0,
	.vinp        = OPAMP3_CSR_VP_SEL_DAC3CH2,
	.inverted    = true,
	.calibrated  = false,
};

static bl_acq_opamp_t bl_acq_opamp4 =
{
	.base        = OPAMP4,
	.dac         = &bl_acq_dac4,
	.dac_channel = DAC_CHANNEL1,
	.adc         = &bl_acq_adc5,
	.adc_channel = 5,
	.enable      = 0,
	.vinp        = OPAMP4_CSR_VP_SEL_DAC4CH1,
	.inverted    = true,
	.calibrated  = false,
};

static bl_acq_opamp_t bl_acq_opamp5 =
{
	.base        = OPAMP5,
	.dac         = &bl_acq_dac4,
	.dac_channel = DAC_CHANNEL2,
	.adc         = &bl_acq_adc5,
	.adc_channel = 3,
	.enable      = 0,
	.vinp        = OPAMP5_CSR_VP_SEL_DAC4CH2,
	.inverted    = true,
	.calibrated  = false,
};

static bl_acq_opamp_t bl_acq_opamp6 =
{
	.base        = OPAMP6,
	.dac         = &bl_acq_dac3,
	.dac_channel = DAC_CHANNEL1,
	.adc         = &bl_acq_adc4,
	.adc_channel = 17,
	.enable      = 0,
	.vinp        = OPAMP6_CSR_VP_SEL_DAC3CH1,
	.inverted    = true,
	.calibrated  = false,
};
#endif


static void bl_acq_opamp_calibrate(bl_acq_opamp_t *opamp)
{
	if (opamp->enable > 0) {
		return;
	}

#if (BL_REVISION >= 2)
	if (opamp->dac) {
		bl_acq_dac_calibrate(opamp->dac);
	}
#endif

	if (opamp->adc) {
		bl_acq_adc_calibrate(opamp->adc);
	}

	rcc_periph_clock_enable(RCC_SYSCFG);

	opamp_enable(opamp->base);

	opamp_user_trim_enable(opamp->base);
	opamp_cal_enable(opamp->base);
	opamp_set_calsel(opamp->base, OPAMP_CSR_CALSEL_90_PERCENT);

	for (unsigned n = 0; n < OPAMP_CSR_TRIMOFFSETN_MASK; n++) {
		opamp_trimoffsetn_set(opamp->base, n);

		bl_delay_us(TOFFTRIM_MAX);

		if (!opamp_read_outcal(opamp->base)) {
			opamp->trimoffsetn = n;
			break;
		}
	}

	opamp_set_calsel(opamp->base, OPAMP_CSR_CALSEL_10_PERCENT);

	for (unsigned p = 0; p < OPAMP_CSR_TRIMOFFSETN_MASK; p++) {
		opamp_trimoffsetp_set(opamp->base, p);

		bl_delay_us(TOFFTRIM_MAX);

		if (!opamp_read_outcal(opamp->base)) {
			opamp->trimoffsetp = p;
			break;
		}
	}

	opamp_user_trim_disable(opamp->base);
	opamp_cal_disable(opamp->base);
	opamp_disable(opamp->base);
}

static enum bl_error bl_acq_opamp_configure(bl_acq_opamp_t *opamp, uint8_t gain)
{
	if (opamp->enable > 0) {
		return BL_ERROR_ACTIVE_ACQUISITION;
	}

	bl_acq_opamp_config_t *config = &opamp->config;

	uint32_t pga_gain;
	if (opamp->inverted) {
		switch (gain) {
#if (BL_REVISION >= 2)
		case 1:
			pga_gain = OPAMP_CSR_PGA_INV_GAIN_MINUS_1_GAIN_2_VM0;
			break;

		case 3:
			pga_gain = OPAMP_CSR_PGA_INV_GAIN_MINUS_3_GAIN_4_VM0;
			break;

		case 7:
			pga_gain = OPAMP_CSR_PGA_INV_GAIN_MINUS_7_GAIN_8_VM0;
			break;

		case 15:
			pga_gain = OPAMP_CSR_PGA_INV_GAIN_MINUS_15_GAIN_16_VM0;
			break;

		case 31:
			pga_gain = OPAMP_CSR_PGA_INV_GAIN_MINUS_31_GAIN_32_VM0;
			break;

		case 63:
			pga_gain = OPAMP_CSR_PGA_INV_GAIN_MINUS_63_GAIN_64_VM0;
			break;
#endif

		default:
			return BL_ERROR_OUT_OF_RANGE;
		}
	} else {
		switch (gain) {
		case 2:
			pga_gain = OPAMP_CSR_PGA_GAIN_2;
			break;

		case 4:
			pga_gain = OPAMP_CSR_PGA_GAIN_4;
			break;

		case 8:
			pga_gain = OPAMP_CSR_PGA_GAIN_8;
			break;

		case 16:
			pga_gain = OPAMP_CSR_PGA_GAIN_16;
			break;

#if (BL_REVISION >= 2)
		case 32:
			pga_gain = OPAMP_CSR_PGA_GAIN_32;
			break;

		case 64:
			pga_gain = OPAMP_CSR_PGA_GAIN_64;
			break;
#endif

		default:
			return BL_ERROR_OUT_OF_RANGE;
		}
	}

	config->pga_gain = pga_gain;
	return BL_ERROR_NONE;
}

static void bl_acq_opamp_enable(bl_acq_opamp_t *opamp)
{
	opamp->enable++;
	if (opamp->enable != 1) {
		/* Already enabled. */
		return;
	}

	rcc_periph_clock_enable(RCC_SYSCFG);

	const bl_acq_opamp_config_t *config = &opamp->config;

#if (BL_REVISION >= 2)
	if (opamp->dac) {
		bl_acq_dac_enable(opamp->dac);
	}
#endif

	if (opamp->calibrated) {
		opamp_user_trim_enable(opamp->base);
		opamp_trimoffsetn_set(opamp->base, opamp->trimoffsetn);
		opamp_trimoffsetp_set(opamp->base, opamp->trimoffsetp);
	}

	opamp_pga_gain_select(opamp->base, config->pga_gain);

	opamp_vm_select(opamp->base, OPAMP_CSR_VM_SEL_PGA_MODE);
	opamp_vp_select(opamp->base, opamp->vinp);

#if (BL_REVISION >= 2)
	opamp_output_set_internal(opamp->base);
#endif

	opamp_enable(opamp->base);

	if (opamp->adc) {
		bl_acq_adc_enable(opamp->adc, 0);
	}
}

static void bl_acq_opamp_disable(bl_acq_opamp_t *opamp)
{
	if (opamp->enable == 0) {
		return;
	}
	opamp->enable--;

	/* Don't disable SYSCFG as it may be used elsewhere. */

	if (opamp->adc) {
		bl_acq_adc_disable(opamp->adc, 0);
	}

	opamp_disable(opamp->base);

#if (BL_REVISION >= 2)
	if (opamp->dac) {
		bl_acq_dac_disable(opamp->dac);
	}
#endif
}



#define OPAMP_OFFSET_NONE 0

typedef struct  {
	bool enable;

	/* Per-channel frequency so we can have per-channel timers in future. */
	uint32_t frequency;

	/* These values are calculated based on variables below. */
	uint32_t frequency_sample;
	uint32_t frequency_trigger;

	/* Gain and offset are parameters for OPAMPs and come with hardware
	 * limitations:
	 * gain: applied to the incoming voltage, power of 2 should be 1..64.
	 * offset: applied to the incoming voltage and is 12-bit. */
	bool     opamp_used;
	uint8_t  opamp_gain;
	uint16_t opamp_offset;

	/* Hardware oversample occurs before anything else, and truncates the
	 * result to 16 bits.
	 * oversample is logarithmic and maxes out at 8.
	 * shift_hw maxes out at 8. */
	uint8_t oversample;
	uint8_t shift;

	/* TODO: Support hardware gain compensation and offset in ADC? */

	uint16_t sw_oversample;
	uint32_t sw_offset;
	uint8_t  sw_shift;

	bool     sample32;
} bl_acq_channel_config_t;

typedef struct
{
	uint32_t        gpio_port;
	uint8_t         gpio_pin;
	bl_acq_opamp_t *opamp;
	bl_acq_adc_t   *adc;
	uint8_t         adc_channel;
	uint32_t        adc_ccr_flag;
	bool            enable;

	union bl_msg_data *msg;

	bl_acq_channel_config_t config;
} bl_acq_channel_t;

static bl_acq_channel_t bl_acq_channel[] =
{
#if (BL_REVISION == 1)
	[BL_ACQ_PD1] = {
		.gpio_port    = GPIOA,
		.gpio_pin     = 4,
		.opamp        = &bl_acq_opamp4,
		.adc          = &bl_acq_adc2,
		.adc_channel  = 1,
		.adc_ccr_flag = 0,
		.enable       = false,
	},
	[BL_ACQ_PD2] = {
		.gpio_port    = GPIOA,
		.gpio_pin     = 3,
		.opamp        = NULL,
		.adc          = &bl_acq_adc1,
		.adc_channel  = 4,
		.adc_ccr_flag = 0,
		.enable       = false,
	},
	[BL_ACQ_PD3] = {
		.gpio_port    = GPIOA,
		.gpio_pin     = 7,
		.opamp        = &bl_acq_opamp2,
		.adc          = &bl_acq_adc2,
		.adc_channel  = 4,
		.adc_ccr_flag = 0,
		.enable       = false,
	},
	[BL_ACQ_PD4] = {
		.gpio_port    = GPIOA,
		.gpio_pin     = 5,
		.opamp        = NULL,
		.adc          = &bl_acq_adc2,
		.adc_channel  = 2,
		.adc_ccr_flag = 0,
		.enable       = false,
	},
	[BL_ACQ_3V3] = {
		.gpio_port    = GPIOA,
		.gpio_pin     = 1,
		.opamp        = NULL,
		.adc          = &bl_acq_adc1,
		.adc_channel  = 2,
		.adc_ccr_flag = 0,
		.enable       = false,
	},
	[BL_ACQ_5V0] = {
		.gpio_port    = GPIOA,
		.gpio_pin     = 2,
		.opamp        = NULL,
		.adc          = &bl_acq_adc1,
		.adc_channel  = 3,
		.adc_ccr_flag = 0,
		.enable       = false,
	},
	[BL_ACQ_TMP] = {
		.gpio_port    = 0,
		.opamp        = NULL,
		.adc          = &bl_acq_adc1,
		.adc_channel  = ADC_CHANNEL_TEMP,
		.adc_ccr_flag = ADC_CCR_TSEN,
		.enable       = false,
	},
#else
	[BL_ACQ_PD1] = {
		.gpio_port    = GPIOB,
		.gpio_pin     = 2,
		.opamp        = &bl_acq_opamp3,
		.adc          = NULL,
		.enable       = false,
	},
	[BL_ACQ_PD2] = {
		.gpio_port    = GPIOB,
		.gpio_pin     = 10,
		.opamp        = &bl_acq_opamp4,
		.adc          = NULL,
		.enable       = false,
	},
	[BL_ACQ_PD3] = {
		.gpio_port    = GPIOB,
		.gpio_pin     = 1,
		.opamp        = &bl_acq_opamp6,
		.adc          = NULL,
		.enable       = false,
	},
	[BL_ACQ_PD4] = {
		.gpio_port    = GPIOA,
		.gpio_pin     = 3,
		.opamp        = &bl_acq_opamp1,
		.adc          = NULL,
		.enable       = false,
	},
	[BL_ACQ_3V3] = {
		.gpio_port    = 0,
		.opamp        = NULL,
		.adc          = &bl_acq_adc1,
		.adc_channel  = ADC_CHANNEL_VBAT,
		.adc_ccr_flag = ADC_CCR_VBATEN,
		.enable       = false,
	},
	[BL_ACQ_5V0] = {
		.gpio_port    = GPIOA,
		.gpio_pin     = 7,
		.opamp        = &bl_acq_opamp2,
		.adc          = &bl_acq_adc2,
		.adc_channel  = 4,
		.adc_ccr_flag = 0,
		.enable       = false,
	},
	[BL_ACQ_TMP] = {
		.gpio_port    = 0,
		.opamp        = NULL,
		.adc          = &bl_acq_adc1,
		.adc_channel  = ADC_CHANNEL_TEMP,
		.adc_ccr_flag = ADC_CCR_TSEN,
		.enable       = false,
	},
	[BL_ACQ_EXT] = {
		.gpio_port    = GPIOB,
		.gpio_pin     = 15,
		.opamp        = &bl_acq_opamp5,
		.adc          = NULL,
		.enable       = false,
	},
#endif
};


static void bl_acq_channel_calibrate(bl_acq_channel_t *channel)
{
	if (channel->opamp) {
		bl_acq_opamp_calibrate(channel->opamp);
	}

	if (channel->adc) {
		bl_acq_adc_calibrate(channel->adc);
	}
}

static void bl_acq_channel_enable(bl_acq_channel_t *channel)
{
	if (channel->enable) {
		/* Already enabled. */
		return;
	}

	if (channel->gpio_port != 0) {
		gpio_mode_setup(channel->gpio_port, GPIO_MODE_ANALOG,
				GPIO_PUPD_NONE, (1U << channel->gpio_pin));
	}

	const bl_acq_channel_config_t *config = &channel->config;

	if (config->opamp_used && (channel->opamp != NULL)) {
		bl_acq_opamp_enable(channel->opamp);
	} else if (channel->adc != NULL) {
		bl_acq_adc_enable(channel->adc, channel->adc_ccr_flag);
	}

	channel->enable = true;
}

static void bl_acq_channel_disable(bl_acq_channel_t *channel)
{
	if ((channel->adc != NULL) && (channel->config.opamp_gain <= 1)) {
		bl_acq_adc_disable(channel->adc, channel->adc_ccr_flag);
	} else if (channel->opamp != NULL) {
		bl_acq_opamp_disable(channel->opamp);
	}

	/* Don't need to do anything here to disable GPIO (it is floating). */

	channel->enable = false;
}

static inline unsigned bl_acq_channel_index(const bl_acq_channel_t *channel)
{
	return (unsigned)(((uintptr_t)channel - (uintptr_t)bl_acq_channel) /
		sizeof(*channel));
}

static bl_acq_timer_t * bl_acq_channel_get_timer(bl_acq_channel_t *channel)
{
	if (!channel->config.enable) {
		return NULL;
	}

	if (channel->config.opamp_used) {
		return channel->opamp->adc->timer;
	}

	return channel->adc->timer;
}

static bl_acq_opamp_t * bl_acq_channel_get_opamp(bl_acq_channel_t *channel)
{
	if (!channel->config.enable || !channel->config.opamp_used) {
		return NULL;
	}

	return channel->opamp;
}

#if (BL_REVISION >= 2)
static bl_acq_dac_t * bl_acq_channel_get_dac(bl_acq_channel_t *channel,
	uint8_t *dac_channel)
{
	bl_acq_opamp_t *opamp = bl_acq_channel_get_opamp(channel);
	if (!opamp) {
		return NULL;
	}

	if (dac_channel != NULL) {
		*dac_channel = channel->opamp->dac_channel;
	}
	return channel->opamp->dac;
}
#endif

static bl_acq_adc_t * bl_acq_channel_get_adc(bl_acq_channel_t *channel,
	uint8_t *adc_channel)
{
	if (!channel->config.enable) {
		return NULL;
	}

	if (channel->config.opamp_used) {
		if (adc_channel != NULL) {
			*adc_channel = channel->opamp->adc_channel;
		}
		return channel->opamp->adc;
	}

	if (adc_channel != NULL) {
		*adc_channel = channel->adc_channel;
	}
	return channel->adc;
}



/* Exported function, documented in acq.h */
void bl_acq_init(uint32_t clock)
{
	rcc_periph_clock_enable(RCC_SYSCFG);

	bl_mq_init();

	/* TODO: Differentiate clock frequencies in input. */
	bl_acq__adc_freq = clock;
	bl_acq__ahb_freq = clock;

	/* Configure ADC groups to get ADC clocks for calibration. */
	bool async = (bl_acq__adc_freq != bl_acq__ahb_freq);
	bl_acq_adc_group_configure(&bl_acq_adc_group12,
			bl_acq__adc_freq, async, false);
#if (BL_REVISION == 1)
	bl_acq_adc_group_configure(&bl_acq_adc_group34,
			bl_acq__adc_freq, async, false);
#else
	bl_acq_adc_group_configure(&bl_acq_adc_group345,
			bl_acq__adc_freq, async, false);
#endif

	for (unsigned src = 0; src < BL_ACQ__SRC_COUNT; src++) {
		bl_acq_channel_t *channel = &bl_acq_channel[src];

		channel->config.enable = true;

		bl_acq_adc_t *adc = bl_acq_channel_get_adc(channel, NULL);

		bl_acq_channel_calibrate(&bl_acq_channel[src]);

		channel->config.enable = false;
	}

}

/* Exported function, documented in acq.h */
enum bl_error bl_acq_start(
		uint16_t frequency,
		uint32_t oversample,
		uint16_t src_mask)
{
	if (src_mask == 0x00) {
		return BL_ERROR_BAD_SOURCE_MASK;
	}

	if (src_mask >= (1U << BL_ACQ__SRC_COUNT)) {
		return BL_ERROR_BAD_SOURCE_MASK;
	}

	if (bl_acq_is_active) {
		return BL_ERROR_ACTIVE_ACQUISITION;
	}

	for (unsigned i = 0; i < BL_ACQ__SRC_COUNT; i++) {
		bl_acq_channel_t *channel = &bl_acq_channel[i];
		bl_acq_channel_config_t *config = &channel->config;

		/* Finalize Config. */
		config->enable = ((src_mask & (1U << i)) != 0);
		if (!config->enable) {
			continue;
		}

		config->frequency     = frequency;
		config->sw_oversample = oversample;
		config->oversample    = 0;

		config->frequency_trigger = (config->frequency *
				config->sw_oversample);
		config->frequency_sample = (config->frequency_trigger <<
				config->oversample);

		bool opamp_needed = (config->opamp_gain > 1) ||
			(config->opamp_offset != OPAMP_OFFSET_NONE);
		if (opamp_needed && (channel->opamp == NULL)) {
			return BL_ERROR_HARDWARE_CONFLICT;
		}

		config->opamp_used = opamp_needed || (channel->adc == NULL);

		/* Initialize message queue. */
		channel->msg = bl_mq_acquire(i);
		if (channel->msg != NULL) {
			channel->msg->type = channel->config.sample32 ?
				BL_MSG_SAMPLE_DATA32 : BL_MSG_SAMPLE_DATA16;
			channel->msg->sample_data.channel  = i;
			channel->msg->sample_data.count    = 0;
			channel->msg->sample_data.reserved = 0x00;
		}
	}

	/* Configure timer(s). */
	for (unsigned i = 0; i < BL_ACQ__SRC_COUNT; i++) {
		bl_acq_channel_t *channel = &bl_acq_channel[i];
		bl_acq_timer_t *timer = bl_acq_channel_get_timer(channel);

		if (timer == NULL) {
			continue;
		}

		/* Ensure same frequency for all users of the timer. */
		for (unsigned j = (i + 1); j < BL_ACQ__SRC_COUNT; j++) {
			bl_acq_channel_t *cb = &bl_acq_channel[j];
			if (bl_acq_channel_get_timer(cb) != timer) {
				continue;
			}

			if (channel->config.frequency_trigger !=
					cb->config.frequency_trigger) {
				return BL_ERROR_HARDWARE_CONFLICT;
			}
		}

		enum bl_error status = bl_acq_timer_configure(timer,
				channel->config.frequency_trigger);
		if (status != BL_ERROR_NONE) {
			return status;
		}
	}

#if (BL_REVISION >= 2)
	/* Configure DACs. */
	for (unsigned i = 0; i < BL_ACQ__SRC_COUNT; i++) {
		bl_acq_channel_t *channel = &bl_acq_channel[i];

		uint8_t dac_channel;
		bl_acq_dac_t *dac = bl_acq_channel_get_dac(
			channel, &dac_channel);
		if (dac == NULL) {
			continue;
		}

		/* Ensure same offsets for all users of same DAC channel. */
		for (unsigned j = (i + 1); j < BL_ACQ__SRC_COUNT; j++) {
			bl_acq_channel_t *channel_b = &bl_acq_channel[j];

			uint8_t dac_channel_b;
			bl_acq_dac_t *dac_b = bl_acq_channel_get_dac(
					channel_b, &dac_channel_b);

			if ((dac_b != dac) || (dac_channel_b != dac_channel)) {
				continue;
			}

			if (channel->config.opamp_offset !=
				channel_b->config.opamp_offset) {
				return BL_ERROR_HARDWARE_CONFLICT;
			}
		}

		enum bl_error status = bl_acq_dac_channel_configure(
			dac, dac_channel, channel->config.opamp_offset);
		if (status != BL_ERROR_NONE) {
			return status;
		}
	}
#endif

	/* Configure OPAMPs. */
	for (unsigned i = 0; i < BL_ACQ__SRC_COUNT; i++) {
		bl_acq_channel_t *channel = &bl_acq_channel[i];

		bl_acq_opamp_t *opamp = bl_acq_channel_get_opamp(channel);
		if (opamp == NULL) {
			continue;
		}

		/* OPAMPs are never shared so no conflicts possible. */

		enum bl_error status = bl_acq_opamp_configure(opamp,
				channel->config.opamp_gain);
		if (status != BL_ERROR_NONE) {
			return status;
		}
	}

	/* Configure ADC groups so we know the frequency. */
	bool async = (bl_acq__adc_freq != bl_acq__ahb_freq);

	bool multi = false;
	bl_acq_adc_t *first_adc = NULL;
	for (unsigned i = 0; i < BL_ACQ__SRC_COUNT; i++) {
		bl_acq_channel_t *channel = &bl_acq_channel[i];

		bl_acq_adc_t *chan_adc = bl_acq_channel_get_adc(channel, NULL);
		if (chan_adc == NULL) {
			continue;
		}

		if (first_adc && (chan_adc != first_adc)) {
			multi = true;
			break;
		}
		first_adc = chan_adc;
	}

	for (unsigned i = 0; i < BL_ACQ__SRC_COUNT; i++) {
		bl_acq_channel_t *channel = &bl_acq_channel[i];
		bl_acq_adc_t *adc = bl_acq_channel_get_adc(channel, NULL);

		if ((adc == NULL) || (adc->group == NULL)) {
			continue;
		}

		enum bl_error status = bl_acq_adc_group_configure(
				adc->group, bl_acq__adc_freq, async, multi);
		if (status != BL_ERROR_NONE) {
			return status;
		}
	}

	/* Configure ADC Channels. */
	for (unsigned i = 0; i < BL_ACQ__SRC_COUNT; i++) {
		bl_acq_channel_t *channel = &bl_acq_channel[i];

		uint8_t adc_channel;
		bl_acq_adc_t *adc = bl_acq_channel_get_adc(
				channel, &adc_channel);
		if (adc == NULL) {
			continue;
		}

		/* Ensure same ADC configs for all users of same ADC. */
		for (unsigned j = (i + 1); j < BL_ACQ__SRC_COUNT; j++) {
			bl_acq_channel_t *channel_b = &bl_acq_channel[j];

			bl_acq_adc_t *adc_b = bl_acq_channel_get_adc(
					channel_b, NULL);

			if (adc_b != adc) {
				continue;
			}

			if (channel->config.oversample !=
				channel_b->config.oversample) {
				return BL_ERROR_HARDWARE_CONFLICT;
			}

			if (channel->config.shift != channel_b->config.shift) {
				return BL_ERROR_HARDWARE_CONFLICT;
			}
		}

		enum bl_error status = bl_acq_adc_channel_configure(
				adc, adc_channel, i);
		if (status != BL_ERROR_NONE) {
			return status;
		}
	}

	/* Configure ADCs. */
	for (unsigned i = 0; i < BL_ACQ__SRC_COUNT; i++) {
		bl_acq_channel_t *channel = &bl_acq_channel[i];

		uint8_t adc_channel;
		bl_acq_adc_t *adc = bl_acq_channel_get_adc(
				channel, &adc_channel);
		if (adc == NULL) {
			continue;
		}

		enum bl_error status = bl_acq_adc_configure(adc,
				channel->config.frequency_sample,
				channel->config.sw_oversample,
				channel->config.oversample,
				channel->config.shift);
		if (status != BL_ERROR_NONE) {
			return status;
		}
	}

	/* Enable all of the channels. */
	for (unsigned i = 0; i < BL_ACQ__SRC_COUNT; i++) {
		bl_acq_channel_t *channel = &bl_acq_channel[i];

		if (channel->config.enable) {
			bl_acq_channel_enable(channel);
		}
	}

	/* Start timer(s) to begin acquisition. */
	bl_acq_is_active = true;
	bl_acq_timer_start_all();

	return BL_ERROR_NONE;
}

/* Exported function, documented in acq.h */
enum bl_error bl_acq_channel_conf(
		uint8_t  index,
		uint8_t  gain,
		uint8_t  shift,
		uint32_t offset,
		bool     sample32)
{
	bl_acq_channel_t *channel = &bl_acq_channel[index];
	bl_acq_channel_config_t *config = &channel->config;

	if (channel->enable) {
		return BL_ERROR_ACTIVE_ACQUISITION;
	}

	/* Config setting is incomplete here so validation is done later. */

	config->opamp_gain   = gain;
	config->opamp_offset = OPAMP_OFFSET_NONE;
	config->shift        = 0;
	config->sw_shift     = shift;
	config->sw_offset    = offset;
	config->sample32     = sample32;

	return BL_ERROR_NONE;
}

/* Exported function, documented in acq.h */
enum bl_error bl_acq_abort(void)
{
	/* Stop acquisition timer(s). */
	bl_acq_timer_stop_all();
	bl_acq_is_active = false;

	/* Disable all channels. */
	for (unsigned i = 0; i < BL_ARRAY_LEN(bl_acq_channel); i++) {
		bl_acq_channel_t *channel = &bl_acq_channel[i];
		if (!channel->enable) {
			continue;
		}

		channel->config.enable = false;

		bl_acq_channel_disable(channel);

		/* Send remaining queued samples. */
		union bl_msg_data *msg = channel->msg;
		if (msg == NULL) {
			continue;
		}

		bool pending;
		switch (msg->type) {
		case BL_MSG_SAMPLE_DATA16:
		case BL_MSG_SAMPLE_DATA32:
			pending = (msg->sample_data.count > 0);
			break;
		default:
			pending = false;
			break;
		}

		if (pending) {
			bl_mq_commit(i);
		}

		channel->msg = NULL;
	}

	return BL_ERROR_NONE;
}


static inline uint16_t bl_acq_sample_pack16(
		uint32_t sample, uint32_t offset, uint8_t shift)
{
	if (sample < offset) return 0;
	sample -= offset;
	sample >>= shift;
	return (sample > 0xFFFF) ? 0xFFFF : sample;
}

static inline void bl_acq_channel_commit_sample_by_index(
		unsigned index, uint32_t sample)
{
	bl_acq_channel_t *channel = &bl_acq_channel[index];
	const bl_acq_channel_config_t *config = &channel->config;

	union bl_msg_data *msg = channel->msg;
	/* Note: We don't check for NULL due to cost. */

	if (config->sample32) {
		uint8_t count = msg->sample_data.count++;
		msg->sample_data.data32[count] = sample;

		if (msg->sample_data.count >= MSG_SAMPLE_DATA32_MAX) {
			bl_mq_commit(index);
			msg = bl_mq_acquire(index);

			/* Note: We dont' check for failure to acquire a msg
			 * due to the cost of checking in an interrupt. */

			msg->type = BL_MSG_SAMPLE_DATA32;
			msg->sample_data.channel  = index;
			msg->sample_data.count    = 0;
			msg->sample_data.reserved = 0;

			channel->msg = msg;
		}
	} else {
		uint8_t count = msg->sample_data.count++;
		uint16_t sample16 = bl_acq_sample_pack16(sample,
				config->sw_offset, config->sw_shift);
		msg->sample_data.data16[count] = sample16;

		if (msg->sample_data.count >= MSG_SAMPLE_DATA16_MAX) {
			bl_mq_commit(index);
			msg = bl_mq_acquire(index);

			/* Note: We dont' check for failure to acquire a msg
			 * due to the cost of checking in an interrupt. */

			msg->type = BL_MSG_SAMPLE_DATA16;
			msg->sample_data.channel  = index;
			msg->sample_data.count    = 0;
			msg->sample_data.reserved = 0;

			channel->msg = msg;
		}
	}
}

static inline void bl_acq_adc_dma_isr(bl_acq_adc_t *adc, unsigned buffer)
{
	volatile uint16_t *p = &adc->dma_buffer[adc->samples_per_dma * buffer];

	uint32_t sample[adc->config.channel_count];
	for (unsigned c = 0; c < adc->config.channel_count; c++) {
		sample[c] = 0;
	}

	for (unsigned s = 0; s < adc->samples_per_dma;) {
		for (unsigned c = 0; c < adc->config.channel_count; c++, s++) {
			sample[c] += *p++;
		}
	}

	for (unsigned c = 0; c < adc->config.channel_count; c++) {
		bl_acq_channel_commit_sample_by_index(
				adc->config.channel_index[c], sample[c]);
	}
}

/* This is a macro to avoid repeated code for each ISR. */
#define DMA_CHANNEL_ISR(__dma, __channel, __adc) \
	void dma##__dma##_channel##__channel##_isr(void) \
	{ \
		if (dma_get_interrupt_flag(DMA##__dma, \
				DMA_CHANNEL##__channel, DMA_HTIF)) { \
			bl_acq_adc_dma_isr(&bl_acq_adc##__adc, 0); \
			dma_clear_interrupt_flags(DMA##__dma, \
					DMA_CHANNEL##__channel, DMA_HTIF); \
		} else { \
			bl_acq_adc_dma_isr(&bl_acq_adc##__adc, 1); \
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

