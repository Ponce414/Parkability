/*
 * sensor-node main.c
 *
 * Responsibilities:
 *   1. Poll the VL53L0X ToF sensor at POLL_INTERVAL_MS
 *   2. Debounce raw readings into a stable SpotState
 *   3. On state change, stamp a Lamport timestamp and send SensorUpdate
 *      over ESP-NOW to the zone leader
 *   4. On boot, send MSG_REGISTER so the leader knows we exist
 *
 * Runs as a single FreeRTOS task. No WiFi — sensors only ever speak ESP-NOW.
 */

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_idf_version.h"
#include "driver/i2c.h"

#include "config.h"
#include "messages.h"
#include "lamport.h"
#include "spot_id.h"

static const char *TAG = "sensor";

static uint8_t g_leader_mac[6] = LEADER_MAC;
static char    g_spot_id[SPOT_ID_MAX_LEN];
static uint8_t g_my_mac[6];

/* Debounce ring buffer. True = reading indicates OCCUPIED. */
static uint8_t  g_window[DEBOUNCE_WINDOW];
static uint8_t  g_window_idx = 0;
static uint8_t  g_window_filled = 0;
static SpotState g_last_state = SPOT_UNKNOWN;
static uint32_t g_last_telemetry_ms = 0;
static uint32_t g_last_range_error_log_ms = 0;

/* ------------------------------------------------------------------------
 * I2C + VL53L0X
 *
 * We talk to the VL53L0X over raw I2C rather than pull in a C++ library to
 * keep this file C-only. The init sequence below is the standard "use
 * factory defaults" path; for higher-accuracy modes see the datasheet.
 * ------------------------------------------------------------------------ */

static esp_err_t i2c_init(void)
{
    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ,
    };
    esp_err_t err = i2c_param_config(I2C_NUM_0, &cfg);
    if (err != ESP_OK) return err;
    return i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
}

static esp_err_t vl53l0x_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2c_master_write_to_device(I2C_NUM_0, VL53L0X_ADDR, buf, 2, pdMS_TO_TICKS(100));
}

static esp_err_t vl53l0x_read_range_mm(uint16_t *out_mm)
{
    /* VL53L0X single-shot range. Write 0x01 to SYSRANGE_START (0x00),
     * poll 0x14 for data ready, read the 16-bit range from 0x1E-0x1F. */
    esp_err_t err = vl53l0x_write_reg(0x00, 0x01);
    if (err != ESP_OK) return err;

    uint8_t status = 0;
    uint8_t reg = 0x14;
    for (int i = 0; i < 20; i++) {
        err = i2c_master_write_read_device(I2C_NUM_0, VL53L0X_ADDR,
                                           &reg, 1, &status, 1, pdMS_TO_TICKS(50));
        if (err != ESP_OK) return err;
        if (status & 0x07) break;
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    uint8_t range_reg = 0x1E;
    uint8_t range_buf[2] = {0, 0};
    err = i2c_master_write_read_device(I2C_NUM_0, VL53L0X_ADDR,
                                       &range_reg, 1, range_buf, 2, pdMS_TO_TICKS(50));
    if (err != ESP_OK) return err;

    *out_mm = ((uint16_t)range_buf[0] << 8) | range_buf[1];
    return ESP_OK;
}

static esp_err_t vl53l0x_init(void)
{
    /* The VL53L0X accepts ranging commands out of the box with reasonable defaults.
     * For bring-up we rely on those; tune accuracy/timing-budget later if needed.
     * See config.h:VL53L0X_TIMING_BUDGET_US — currently informational only. */
    ESP_LOGI(TAG, "VL53L0X assumed initialized with factory defaults");
    return ESP_OK;
}

/* ------------------------------------------------------------------------
 * WiFi + ESP-NOW
 * ------------------------------------------------------------------------ */

static void wifi_init_for_espnow(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wcfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));
}

static void log_espnow_send_failure(const uint8_t *mac)
{
    if (mac) {
        ESP_LOGW(TAG, "espnow send failed to %02x:%02x:%02x:%02x:%02x:%02x",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    } else {
        ESP_LOGW(TAG, "espnow send failed");
    }
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
static void on_espnow_send(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)
{
    if (status != ESP_NOW_SEND_SUCCESS) {
        log_espnow_send_failure(tx_info ? tx_info->des_addr : NULL);
    }
}
#else
static void on_espnow_send(const uint8_t *mac, esp_now_send_status_t status)
{
    if (status != ESP_NOW_SEND_SUCCESS) {
        log_espnow_send_failure(mac);
    }
}
#endif

/* Forward declaration; defined below. */
static void send_register(void);

static void switch_leader(const uint8_t new_leader_mac[6])
{
    /* Remove old peer (ignore error — peer table may be empty after boot
     * mishaps). Add new peer and re-send REGISTER so the new leader knows
     * about us. Idempotent on the leader side. */
    esp_now_del_peer(g_leader_mac);
    memcpy(g_leader_mac, new_leader_mac, 6);

    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, g_leader_mac, 6);
    peer.channel = ESPNOW_CHANNEL;
    peer.ifidx   = WIFI_IF_STA;
    peer.encrypt = false;
    esp_now_add_peer(&peer);

    ESP_LOGI(TAG, "switched leader to %02x:%02x:%02x:%02x:%02x:%02x",
             g_leader_mac[0], g_leader_mac[1], g_leader_mac[2],
             g_leader_mac[3], g_leader_mac[4], g_leader_mac[5]);
    send_register();
}

static void on_espnow_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    if (len < (int)sizeof(MessageHeader)) return;
    const MessageHeader *hdr = (const MessageHeader *)data;
    lamport_update(hdr->lamport_ts);

    switch (hdr->msg_type) {
    case MSG_REGISTER_ACK:
        if (len >= (int)sizeof(RegisterAck)) {
            const RegisterAck *ack = (const RegisterAck *)data;
            ESP_LOGI(TAG, "register ack for %s accepted=%u", ack->spot_id, ack->accepted);
        }
        break;
    case MSG_COORDINATOR:
        /* Bully announced a new leader. Re-pair and re-register. The new
         * coordinator's MAC is the sender of this message. */
        if (len >= (int)sizeof(Coordinator)) {
            switch_leader(hdr->sender_mac);
        }
        break;
    default:
        /* Sensors don't act on heartbeats, elections, or sensor updates. */
        break;
    }
}

static esp_err_t espnow_init(void)
{
    esp_err_t err = esp_now_init();
    if (err != ESP_OK) return err;
    esp_now_register_send_cb(on_espnow_send);
    esp_now_register_recv_cb(on_espnow_recv);

    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, g_leader_mac, 6);
    peer.channel = ESPNOW_CHANNEL;
    peer.ifidx   = WIFI_IF_STA;
    peer.encrypt = false;
    err = esp_now_add_peer(&peer);
    return err;
}

static void send_register(void)
{
    Register msg = {0};
    msg.header.protocol_version = PROTOCOL_VERSION;
    msg.header.msg_type = MSG_REGISTER;
    memcpy(msg.header.sender_mac, g_my_mac, 6);
    msg.header.lamport_ts = lamport_tick();
    strncpy(msg.spot_id, g_spot_id, SPOT_ID_MAX_LEN - 1);
    msg.firmware_version = FIRMWARE_VERSION;
    esp_now_send(g_leader_mac, (const uint8_t *)&msg, sizeof(msg));
}

static void send_sensor_update(SpotState state, uint16_t distance_mm)
{
    SensorUpdate msg = {0};
    msg.header.protocol_version = PROTOCOL_VERSION;
    msg.header.msg_type = MSG_SENSOR_UPDATE;
    memcpy(msg.header.sender_mac, g_my_mac, 6);
    msg.header.lamport_ts = lamport_tick();
    strncpy(msg.spot_id, g_spot_id, SPOT_ID_MAX_LEN - 1);
    msg.state = (uint8_t)state;
    msg.raw_distance_mm = distance_mm;
    msg.wall_ts = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    esp_now_send(g_leader_mac, (const uint8_t *)&msg, sizeof(msg));
}

/* ------------------------------------------------------------------------
 * Debounce
 *
 * Require DEBOUNCE_COUNT of the last DEBOUNCE_WINDOW readings to agree
 * before transitioning state. Rejects jittery readings at the threshold
 * without adding big latency.
 * ------------------------------------------------------------------------ */

static SpotState debounce_step(uint8_t raw_occupied)
{
    g_window[g_window_idx] = raw_occupied;
    g_window_idx = (g_window_idx + 1) % DEBOUNCE_WINDOW;
    if (g_window_idx == 0) g_window_filled = 1;

    uint8_t window_len = g_window_filled ? DEBOUNCE_WINDOW : g_window_idx;
    if (window_len < DEBOUNCE_COUNT) return g_last_state;

    uint8_t occ = 0, free = 0;
    for (uint8_t i = 0; i < window_len; i++) {
        if (g_window[i]) occ++; else free++;
    }
    if (occ  >= DEBOUNCE_COUNT) return SPOT_OCCUPIED;
    if (free >= DEBOUNCE_COUNT) return SPOT_FREE;
    return g_last_state;
}

/* ------------------------------------------------------------------------
 * Main task
 * ------------------------------------------------------------------------ */

static void sensor_task(void *arg)
{
    while (1) {
        uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
        uint16_t mm = 0;
        esp_err_t err = vl53l0x_read_range_mm(&mm);
        if (err != ESP_OK) {
            if ((uint32_t)(now_ms - g_last_range_error_log_ms) >= TELEMETRY_INTERVAL_MS) {
                ESP_LOGW(TAG, "range read failed: %d", err);
                g_last_range_error_log_ms = now_ms;
            }
            if ((uint32_t)(now_ms - g_last_telemetry_ms) >= TELEMETRY_INTERVAL_MS) {
                ESP_LOGI(TAG, "telemetry refresh state=%d (range failed)", SPOT_UNKNOWN);
                send_sensor_update(SPOT_UNKNOWN, 0);
                g_last_telemetry_ms = now_ms;
            }
            vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
            continue;
        }

        uint8_t raw_occupied = (mm > 0 && mm < OCCUPANCY_THRESHOLD_MM) ? 1 : 0;
        SpotState new_state = debounce_step(raw_occupied);

        if (new_state != SPOT_UNKNOWN &&
            (new_state != g_last_state ||
             (uint32_t)(now_ms - g_last_telemetry_ms) >= TELEMETRY_INTERVAL_MS)) {
            if (new_state != g_last_state) {
                ESP_LOGI(TAG, "state change %d -> %d (%u mm)", g_last_state, new_state, mm);
            } else {
                ESP_LOGI(TAG, "telemetry refresh state=%d (%u mm)", new_state, mm);
            }
            g_last_state = new_state;
            send_sensor_update(new_state, mm);
            g_last_telemetry_ms = now_ms;
        }

        vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
    }
}

void app_main(void)
{
    /* NVS is required by the WiFi stack. */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    lamport_init();
    spot_id_format(g_spot_id, SPOT_LOT, SPOT_ZONE, SPOT_SPOT);
    ESP_LOGI(TAG, "booting as %s", g_spot_id);

    ESP_ERROR_CHECK(i2c_init());
    ESP_ERROR_CHECK(vl53l0x_init());

    wifi_init_for_espnow();
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, g_my_mac));
    ESP_ERROR_CHECK(espnow_init());

    /* Announce ourselves to the leader. The leader de-dupes by spot_id so
     * repeated REGISTERs on reboot are harmless. */
    send_register();

    xTaskCreate(sensor_task, "sensor", 4096, NULL, 5, NULL);
}
