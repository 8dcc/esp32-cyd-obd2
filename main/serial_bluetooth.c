/*
 * Copyright 2025 8dcc
 *
 * This file is part of ESP32 CYD OBD2.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "serial_bluetooth.h"

#include <string.h>

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_spp_api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "util.h"

#define BT_RX_BUF_SIZE        512
#define BT_CONNECT_TIMEOUT_MS 10000

typedef struct {
    uint32_t handle;
    bool connected;
    uint8_t rx_buf[BT_RX_BUF_SIZE];
    size_t rx_head;
    size_t rx_tail;
    SemaphoreHandle_t rx_mutex;
    SemaphoreHandle_t connect_sem;
} SerialBtCtx;

static SerialBtCtx g_bt_ctx;

/*----------------------------------------------------------------------------*/

static void spp_callback(esp_spp_cb_event_t event, esp_spp_cb_param_t* param) {
    switch (event) {
        case ESP_SPP_OPEN_EVT:
            g_bt_ctx.handle    = param->open.handle;
            g_bt_ctx.connected = true;
            xSemaphoreGive(g_bt_ctx.connect_sem);
            LOGI("BT SPP connected, handle=%lu",
                 (unsigned long)g_bt_ctx.handle);
            break;

        case ESP_SPP_DATA_IND_EVT: {
            const uint8_t* data = param->data_ind.data;
            const uint16_t len  = param->data_ind.len;
            xSemaphoreTake(g_bt_ctx.rx_mutex, portMAX_DELAY);
            for (uint16_t i = 0; i < len; i++) {
                size_t next = (g_bt_ctx.rx_head + 1) % BT_RX_BUF_SIZE;
                if (next != g_bt_ctx.rx_tail) { /* drop byte if buffer full */
                    g_bt_ctx.rx_buf[g_bt_ctx.rx_head] = data[i];
                    g_bt_ctx.rx_head                  = next;
                }
            }
            xSemaphoreGive(g_bt_ctx.rx_mutex);
            break;
        }

        case ESP_SPP_CLOSE_EVT:
            g_bt_ctx.connected = false;
            LOGI("BT SPP disconnected");
            break;

        default:
            break;
    }
}

/*----------------------------------------------------------------------------*/

bool serial_bt_init(void) {
    memset(&g_bt_ctx, 0, sizeof(g_bt_ctx));
    g_bt_ctx.rx_mutex    = xSemaphoreCreateMutex();
    g_bt_ctx.connect_sem = xSemaphoreCreateBinary();
    if (g_bt_ctx.rx_mutex == NULL || g_bt_ctx.connect_sem == NULL)
        goto fail_semaphores;

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if (esp_bt_controller_init(&bt_cfg) != ESP_OK)
        goto fail_semaphores;
    if (esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT) != ESP_OK)
        goto fail_controller;
    if (esp_bluedroid_init() != ESP_OK)
        goto fail_controller;
    if (esp_bluedroid_enable() != ESP_OK)
        goto fail_bluedroid;
    if (esp_spp_register_callback(spp_callback) != ESP_OK)
        goto fail_bluedroid;
    if (esp_spp_init(ESP_SPP_MODE_CB) != ESP_OK)
        goto fail_bluedroid;

    LOGI("Bluetooth initialized.");
    return true;

fail_bluedroid:
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
fail_controller:
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
fail_semaphores:
    if (g_bt_ctx.rx_mutex)
        vSemaphoreDelete(g_bt_ctx.rx_mutex);
    if (g_bt_ctx.connect_sem)
        vSemaphoreDelete(g_bt_ctx.connect_sem);
    return false;
}

bool serial_bt_connect(const uint8_t mac[6]) {
    g_bt_ctx.connected = false;

    /* Drain any stale token left from a previous OPEN_EVT */
    xSemaphoreTake(g_bt_ctx.connect_sem, 0);

    esp_bd_addr_t remote_addr;
    memcpy(remote_addr, mac, sizeof(remote_addr));

    if (esp_spp_connect(ESP_SPP_SEC_NONE,
                        ESP_SPP_ROLE_MASTER,
                        1,
                        remote_addr) != ESP_OK) {
        LOGE("esp_spp_connect failed");
        return false;
    }

    /* Block until spp_callback gives the semaphore (OPEN_EVT) or timeout */
    const TickType_t timeout = BT_CONNECT_TIMEOUT_MS / portTICK_PERIOD_MS;
    if (xSemaphoreTake(g_bt_ctx.connect_sem, timeout) != pdTRUE) {
        LOGE("BT connect timeout");
        return false;
    }

    return true;
}

bool serial_bt_is_connected(void) {
    return g_bt_ctx.connected;
}

int serial_bt_write(const uint8_t* data, size_t len) {
    if (!g_bt_ctx.connected)
        return -1;
    /* esp_spp_write takes a non-const pointer; cast is safe here */
    const esp_err_t err = esp_spp_write(g_bt_ctx.handle, len, (uint8_t*)data);
    return (err == ESP_OK) ? (int)len : -1;
}

int serial_bt_read(uint8_t* buf, size_t len) {
    if (xSemaphoreTake(g_bt_ctx.rx_mutex, 0) != pdTRUE)
        return 0;
    size_t count = 0;
    while (count < len && g_bt_ctx.rx_tail != g_bt_ctx.rx_head) {
        buf[count++]     = g_bt_ctx.rx_buf[g_bt_ctx.rx_tail];
        g_bt_ctx.rx_tail = (g_bt_ctx.rx_tail + 1) % BT_RX_BUF_SIZE;
    }
    xSemaphoreGive(g_bt_ctx.rx_mutex);
    return (int)count;
}

void serial_bt_deinit(void) {
    if (g_bt_ctx.connected)
        esp_spp_disconnect(g_bt_ctx.handle);
    esp_spp_deinit();
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    vSemaphoreDelete(g_bt_ctx.rx_mutex);
    vSemaphoreDelete(g_bt_ctx.connect_sem);
}
