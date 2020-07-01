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

#include <stdint.h>
#include <stdio.h>

#include "../src/error.h"
#include "../src/util.h"
#include "../src/msg.h"

#include "msg.h"

static const char *bl_msg_type_to_str(enum bl_msg_type type)
{
	static const char *types[] = {
		[BL_MSG_RESPONSE]        = "Response",
		[BL_MSG_LED]             = "LED",
		[BL_MSG_START]           = "Start",
		[BL_MSG_ABORT]           = "Abort",
		[BL_MSG_SAMPLE_DATA]     = "Sample Data",
		[BL_MSG_SET_GAINS]       = "Set Gains",
		[BL_MSG_SET_OVERSAMPLE]  = "Set Oversample",
		[BL_MSG_SET_FIXEDOFFSET] = "Set Fixed Offset",
	};

	if (type >= BL_ARRAY_LEN(types)) {
		return NULL;
	}

	return types[type];
}

void bl_msg_print(const union bl_msg_data *msg, FILE *file)
{
	if (bl_msg_type_to_str(msg->type) == NULL) {
		fprintf(file, "- Unknown (0x%x)\n",
				(unsigned) msg->type);
		return;
	}

	fprintf(file, "- %s:\n", bl_msg_type_to_str(msg->type));

	switch (msg->type) {
	case BL_MSG_RESPONSE:
		if (bl_msg_type_to_str(msg->response.response_to) == NULL) {
			fprintf(file, "    Response to: Unknown (0x%x)\n",
					(unsigned) msg->response.response_to);
		} else {
			fprintf(file, "    Response to: %s\n",
					bl_msg_type_to_str(
						msg->response.response_to));
		}
		fprintf(file, "    Error code: %u\n",
				(unsigned) msg->response.error_code);
		break;

	case BL_MSG_LED:
		fprintf(file, "    LED Mask: 0x%x\n",
				(unsigned) msg->led.led_mask);
		break;

	case BL_MSG_START:
		fprintf(file, "    Frequency: %u\n",
				(unsigned) msg->start.frequency);
		fprintf(file, "    Source Mask: 0x%x\n",
				(unsigned) msg->start.src_mask);		
		break;

	case BL_MSG_SAMPLE_DATA:
		fprintf(file, "    Count: %u\n",
				(unsigned) msg->sample_data.count);
		fprintf(file, "    Source Mask: 0x%x\n",
				(unsigned) msg->sample_data.src_mask);
		fprintf(file, "    Data:\n");
		for (unsigned i = 0; i < msg->sample_data.count; i++) {
			fprintf(file, "    - %u\n",
				(unsigned) msg->sample_data.data[i]);
		}
		break;
	case BL_MSG_SET_GAINS:
		fprintf(file, "    Gains:\n");
		for (unsigned i = 0; i < BL_ACQ_PD__COUNT; i++) {
			fprintf(file, "    - %u\n",
				(unsigned) msg->gain.gain[i]);
		}
		break;
	case BL_MSG_SET_OVERSAMPLE:
		fprintf(file, "    Oversample: %u\n", msg->oversample.oversample);
		break;
	case BL_MSG_SET_FIXEDOFFSET:
		fprintf(file, "    Offset:\n");
		for (unsigned i = 0; i < BL_ACQ__SRC_COUNT; i++) {
			fprintf(file, "    - %u\n",
				(unsigned) msg->offset.offset[i]);
		}
		break;
	default:
		break;
	}

	fflush(file);
}
