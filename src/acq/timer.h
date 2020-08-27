#ifndef BL_ACQ_TIMER_H
#define BL_ACQ_TIMER_H

#include <stdint.h>

#include "../error.h"


typedef struct bl_acq_timer_s bl_acq_timer_t;

extern bl_acq_timer_t *bl_acq_tim1;

void bl_acq_timer_init(uint32_t bus_freq);

enum bl_error bl_acq_timer_configure(bl_acq_timer_t *timer, uint32_t frequency);

void bl_acq_timer_enable(bl_acq_timer_t *timer);
void bl_acq_timer_disable(bl_acq_timer_t *timer);

void bl_acq_timer_start(bl_acq_timer_t *timer);
void bl_acq_timer_stop(bl_acq_timer_t *timer);

void bl_acq_timer_start_all(void);
void bl_acq_timer_stop_all(void);

#endif

