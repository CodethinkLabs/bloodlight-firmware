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

#include "error.h"
#include "util.h"
#include "acq.h"
#include "msg.h"

/* Exported function, documented in acq.h */
void bl_acq_init(void)
{
	
}

/* Exported function, documented in acq.h */
enum bl_error bl_acq_setup(
		uint16_t rate,
		uint16_t samples,
		const uint8_t gain[BL_LED__COUNT][BL_ACQ_PD__COUNT])
{
	BL_UNUSED(rate);
	BL_UNUSED(samples);
	BL_UNUSED(gain);

	return BL_ERROR_NONE;
}

/* Exported function, documented in acq.h */
enum bl_error bl_acq_start(void)
{
	return BL_ERROR_NONE;
}

/* Exported function, documented in acq.h */
enum bl_error bl_acq_abort(void)
{
	return BL_ERROR_NONE;
}
