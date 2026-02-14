/*
 * Copyright 2025 8dcc
 *
 * This file is part of ESP32 CYD OBD2.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef UTIL_H_
#define UTIL_H_ 1

/*
 * Return the compile-time length of an array.
 */
#define LENGTH(ARR) (sizeof(ARR) / sizeof((ARR)[0]))

/*
 * Wrappers for ESP-IDF's logging macros, passing the current filename as the
 * TAG argument.
 */
#if !defined(LOG_DISABLE)
#include "esp_log.h"
#if !defined(__FILE_NAME__)
#define __FILE_NAME__ __FILE__
#endif /* !defined(__FILE_NAME__) */
#define LOGI(...) ESP_LOGI(__FILE_NAME__, __VA_ARGS__);
#define LOGW(...) ESP_LOGW(__FILE_NAME__, __VA_ARGS__);
#define LOGE(...) ESP_LOGE(__FILE_NAME__, __VA_ARGS__);
#endif /* !defined(LOG_DISABLE) */

#endif /* UTIL_H_ */
