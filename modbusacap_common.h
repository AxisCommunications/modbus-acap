/**
 * Copyright (C) 2023, Axis Communications AB, Lund, Sweden
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

#ifndef _MODBUSACAP_COMMON_H_
#define _MODBUSACAP_COMMON_H_

#include <stdio.h>
#include <syslog.h>

// clang-format off
#define LOG(type, fmt, args...) { syslog(type, fmt, ##args); printf(fmt, ##args); printf("\n"); }
#define LOG_I(fmt, args...) { LOG(LOG_INFO, fmt, ##args) }
#define LOG_E(fmt, args...) { LOG(LOG_ERR, fmt, ##args) }
// clang-format on

#endif /* _MODBUSACAP_COMMON_H_ */
