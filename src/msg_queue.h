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

#ifndef BL_MSG_QUEUE_H
#define BL_MSG_QUEUE_H

#include "msg.h"

void bl_msg_queue_init(void);

union bl_msg_data *bl_msg_queue_acquire(void);
void               bl_msg_queue_commit(union bl_msg_data *msg);
union bl_msg_data *bl_msg_queue_peek(void);
void               bl_msg_queue_release(union bl_msg_data *msg);

#endif
