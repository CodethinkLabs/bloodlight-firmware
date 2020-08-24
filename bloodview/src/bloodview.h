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

#ifndef BLOODVIEW_H
#define BLOODVIEW_H

/**
 * Main menu callback for starting a calibration acquisition.
 *
 * \param[in]  pw  Client private context data.
 */
void bloodview_start_cal_cb(void *pw);

/**
 * Main menu callback for starting an acquisition.
 *
 * \param[in]  pw  Client private context data.
 */
void bloodview_start_acq_cb(void *pw);

/**
 * Main menu callback for stopping acquisition.
 *
 * \param[in]  pw  Client private context data.
 */
void bloodview_stop_cb(void *pw);

/**
 * Main menu callback for quitting Bloodview
 *
 * \param[in]  pw  Client private context data.
 */
void bloodview_quit_cb(void *pw);

#endif /* BLOODVIEW_H */
