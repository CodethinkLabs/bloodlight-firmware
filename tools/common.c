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

static inline const char *bl_msg_type_to_str(enum bl_msg_type type)
{
	static const char *types[] = {
		[BL_MSG_RESPONSE]    = "Response",
		[BL_MSG_LED_TEST]    = "LED Test",
		[BL_MSG_ACQ_SETUP]   = "Acq Setup",
		[BL_MSG_ACQ_START]   = "Acq Start",
		[BL_MSG_ACQ_ABORT]   = "Acq Abort",
		[BL_MSG_RESET]       = "Reset",
		[BL_MSG_SAMPLE_DATA] = "Sample Data",
	};

	if (type >= BL_ARRAY_LEN(types)) {
		return NULL;
	}

	return types[type];
}

static void bl_msg_print(const union bl_msg_data *msg, FILE *file)
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

	case BL_MSG_LED_TEST:
		fprintf(file, "    LED Mask: 0x%x\n",
				(unsigned) msg->led_test.led_mask);
		break;

	case BL_MSG_ACQ_SETUP:
		fprintf(file, "    Oversample: %u\n",
				(unsigned) msg->acq_setup.oversample);
		fprintf(file, "    Period: %u\n",
				(unsigned) msg->acq_setup.period);
		fprintf(file, "    Source Mask: 0x%x\n",
				(unsigned) msg->acq_setup.src_mask);
		fprintf(file, "    Gain:\n");
		for (unsigned i = 0; i < BL_ACQ_PD__COUNT; i++) {
			fprintf(file, "    - %u\n",
				(unsigned) msg->acq_setup.gain[i]);
		}
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

	default:
		break;
	}

	fflush(file);
}
