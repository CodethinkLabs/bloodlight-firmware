#ifndef BL_ACQ_RCC_H
#define BL_ACQ_RCC_H

#include <stdbool.h>
#include <stdint.h>

#include <libopencm3/stm32/rcc.h>

static inline bool bl_acq__rcc_enable_ref(uint32_t rcc_unit, unsigned *ref)
{
	*ref += 1;

	if (*ref == 1) {
		rcc_periph_clock_enable(rcc_unit);
		return true;
	}

	return false;
}

static inline bool bl_acq__rcc_disable_ref(uint32_t rcc_unit, unsigned *ref)
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

#endif

