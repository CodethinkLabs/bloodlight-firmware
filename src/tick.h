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

#ifndef BL_TICK_H
#define BL_TICK_H

/**
 * Initialise ticking
 */
void bl_tick_init(void);

/**
 * Set up a usec timer. 
 * 
 * Use @ref bl_get_us_timer_elapsed to get the time elapsed since setting
 * 
 * \param[in]  timer  Timer to be used
 */
void bl_set_us_timer(uint32_t * timer);

/**
 * Get the elapsed time for a usec timer
 * 
 * The time is the usecs since  @ref bl_set_us_timer  was called on the timer
 * 
 * \param[in]  timer    Timer to get the elapsed value of
 * \return     uint32_t usecs elapsed
 */
uint32_t bl_get_us_timer_elapsed(uint32_t timer);

/**
 * Set up a msec timer. 
 * 
 * Use @ref bl_get_ms_timer_elapsed to get the time elapsed since setting
 * 
 * \param[in]  timer  Timer to be used
 */
void bl_set_ms_timer(uint32_t * timer);

/**
 * Get the elapsed time for a msec timer
 * 
 * The time is the msecs since  @ref bl_set_us_timer  was called on the timer
 * 
 * \param[in]  timer    Timer to get the elapsed value of
 * \return     uint32_t msecs elapsed
 */
uint32_t bl_get_ms_timer_elapsed(uint32_t timer);

#endif // BL_TICK_H