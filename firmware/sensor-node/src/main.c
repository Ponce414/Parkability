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
static const uint8_t BROADCAST_MAC[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static char    g_spot_id[SPOT_ID_MAX_LEN];
static uint8_t g_my_mac[6];

/* Debounce ring buffer. True = reading indicates OCCUPIED. */
static uint8_t  g_window[DEBOUNCE_WINDOW];
static uint8_t  g_window_idx = 0;
static uint8_t  g_window_filled = 0;
static SpotState g_last_state = SPOT_UNKNOWN;
static uint32_t g_last_telemetry_ms = 0;
static uint32_t g_last_range_error_log_ms = 0;
static uint16_t g_last_reported_distance_mm = 0;
static uint16_t g_distance_window[DISTANCE_FILTER_WINDOW];
static uint8_t  g_distance_window_idx = 0;
static uint8_t  g_distance_window_len = 0;
static uint16_t g_stable_distance_mm = 0;
static uint16_t g_candidate_distance_mm = 0;
static uint8_t  g_candidate_distance_count = 0;
static uint32_t g_last_register_ms = 0;
static uint8_t  g_current_channel = 1;
static uint8_t  g_consecutive_send_failures = 0;
static volatile int g_registered = 0;
static volatile int g_espnow_ready = 0;
static int g_vl53l0x_present = 0;
static int g_i2c_sda_gpio = I2C_SDA_GPIO;
static int g_i2c_scl_gpio = I2C_SCL_GPIO;
static int g_i2c_driver_installed = 0;

typedef struct {
    int sda;
    int scl;
} I2cPinPair;

/* ------------------------------------------------------------------------
 * I2C + VL53L0X
 *
 * We talk to the VL53L0X over raw I2C rather than pull in a C++ library to
 * keep this file C-only. The init sequence below is the standard "use
 * factory defaults" path; for higher-accuracy modes see the datasheet.
 * ------------------------------------------------------------------------ */

static esp_err_t i2c_install_on_pins(int sda_gpio, int scl_gpio)
{
    if (g_i2c_driver_installed) {
        i2c_driver_delete(I2C_NUM_0);
        g_i2c_driver_installed = 0;
    }

    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda_gpio,
        .scl_io_num = scl_gpio,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ,
    };
    esp_err_t err = i2c_param_config(I2C_NUM_0, &cfg);
    if (err != ESP_OK) return err;

    err = i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
    if (err == ESP_OK) {
        g_i2c_sda_gpio = sda_gpio;
        g_i2c_scl_gpio = scl_gpio;
        g_i2c_driver_installed = 1;
    }
    return err;
}

static esp_err_t i2c_init(void)
{
    return i2c_install_on_pins(I2C_SDA_GPIO, I2C_SCL_GPIO);
}

static esp_err_t vl53l0x_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2c_master_write_to_device(I2C_NUM_0, VL53L0X_ADDR, buf, 2, pdMS_TO_TICKS(100));
}

static esp_err_t i2c_probe_addr(uint8_t addr)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (!cmd) return ESP_ERR_NO_MEM;

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(25));
    i2c_cmd_link_delete(cmd);
    return err;
}

static void i2c_scan_bus(void)
{
    int found = 0;
    ESP_LOGI(TAG, "I2C scan on SDA=GPIO%d SCL=GPIO%d freq=%dHz",
             g_i2c_sda_gpio, g_i2c_scl_gpio, I2C_FREQ_HZ);
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        if (i2c_probe_addr(addr) == ESP_OK) {
            found++;
            ESP_LOGI(TAG, "I2C device found at 0x%02x%s",
                     addr, addr == VL53L0X_ADDR ? " (VL53L0X expected)" : "");
        }
    }
    if (found == 0) {
        ESP_LOGW(TAG, "I2C scan found no devices; check 3V3, GND, SDA=GPIO%d, SCL=GPIO%d",
                 g_i2c_sda_gpio, g_i2c_scl_gpio);
    }
}

static esp_err_t vl53l0x_read_reg(uint8_t reg, uint8_t *buf, size_t len)
{
    return i2c_master_write_read_device(I2C_NUM_0, VL53L0X_ADDR,
                                        &reg, 1, buf, len, pdMS_TO_TICKS(100));
}

static esp_err_t vl53l0x_detect(int log_success)
{
    esp_err_t err = i2c_probe_addr(VL53L0X_ADDR);
    if (err != ESP_OK) {
        g_vl53l0x_present = 0;
        ESP_LOGW(TAG, "VL53L0X not responding at 0x%02x on SDA=GPIO%d SCL=GPIO%d: %s",
                 VL53L0X_ADDR, g_i2c_sda_gpio, g_i2c_scl_gpio, esp_err_to_name(err));
        return err;
    }

    uint8_t model_id = 0;
    err = vl53l0x_read_reg(0xC0, &model_id, 1);
    if (err != ESP_OK) {
        g_vl53l0x_present = 0;
        ESP_LOGW(TAG, "VL53L0X ID read failed at 0x%02x on SDA=GPIO%d SCL=GPIO%d: %s",
                 VL53L0X_ADDR, g_i2c_sda_gpio, g_i2c_scl_gpio, esp_err_to_name(err));
        return err;
    }

    g_vl53l0x_present = 1;
    if (log_success) {
        ESP_LOGI(TAG, "VL53L0X detected at 0x%02x model_id=0x%02x on SDA=GPIO%d SCL=GPIO%d",
                 VL53L0X_ADDR, model_id, g_i2c_sda_gpio, g_i2c_scl_gpio);
    }
    return ESP_OK;
}

static esp_err_t vl53l0x_autodetect_bus(void)
{
    esp_err_t err = vl53l0x_detect(1);
    if (err == ESP_OK) return ESP_OK;

#if I2C_AUTODETECT_PINS
    static const I2cPinPair candidates[] = {
        { I2C_SDA_GPIO, I2C_SCL_GPIO },
        { I2C_SCL_GPIO, I2C_SDA_GPIO },
        { 6, 7 }, { 7, 6 },
        { 4, 5 }, { 5, 4 },
        { 2, 3 }, { 3, 2 },
        { 0, 1 }, { 1, 0 },
    };

    ESP_LOGI(TAG, "probing alternate I2C pin pairs for VL53L0X");
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        int sda = candidates[i].sda;
        int scl = candidates[i].scl;
        if (sda == g_i2c_sda_gpio && scl == g_i2c_scl_gpio) continue;

        err = i2c_install_on_pins(sda, scl);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "I2C install failed on SDA=GPIO%d SCL=GPIO%d: %s",
                     sda, scl, esp_err_to_name(err));
            continue;
        }
        err = vl53l0x_detect(1);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "using detected I2C bus SDA=GPIO%d SCL=GPIO%d", sda, scl);
            return ESP_OK;
        }
    }

    i2c_install_on_pins(I2C_SDA_GPIO, I2C_SCL_GPIO);
#endif
    return err;
}

static esp_err_t vl53l0x_read_range_mm(uint16_t *out_mm)
{
    if (!g_vl53l0x_present) return ESP_ERR_NOT_FOUND;

    /* VL53L0X single-shot range. Write 0x01 to SYSRANGE_START (0x00),
     * poll 0x14 for data ready, read the 16-bit range from 0x1E-0x1F. */
    esp_err_t err = vl53l0x_write_reg(0x00, 0x01);
    if (err != ESP_OK) {
        g_vl53l0x_present = 0;
        return err;
    }

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
    i2c_scan_bus();
    vl53l0x_autodetect_bus();
    ESP_LOGI(TAG, "VL53L0X bring-up complete; timing budget target=%dus",
             VL53L0X_TIMING_BUDGET_US);
    return ESP_OK;
}

/* ------------------------------------------------------------------------
 * WiFi + ESP-NOW
 * ------------------------------------------------------------------------ */

static uint8_t initial_espnow_channel(void)
{
#if ESPNOW_CHANNEL == 0
    return 1;
#else
    return ESPNOW_CHANNEL;
#endif
}

static uint8_t next_discovery_channel(void)
{
#if ESPNOW_CHANNEL == 0
    uint8_t next = (uint8_t)(g_current_channel + 1);
    return next > 11 ? 1 : next;
#else
    return ESPNOW_CHANNEL;
#endif
}

static esp_err_t add_espnow_peer(const uint8_t mac[6], uint8_t channel)
{
    esp_now_del_peer(mac);

    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = channel;
    peer.ifidx   = WIFI_IF_STA;
    peer.encrypt = false;
    return esp_now_add_peer(&peer);
}

static esp_err_t add_leader_peer(uint8_t channel)
{
    return add_espnow_peer(g_leader_mac, channel);
}

static esp_err_t add_broadcast_peer(uint8_t channel)
{
    return add_espnow_peer(BROADCAST_MAC, channel);
}

static void refresh_espnow_peers(uint8_t channel)
{
    esp_err_t err = add_leader_peer(channel);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "leader peer add failed on ch=%u: %s",
                 channel, esp_err_to_name(err));
    }

    err = add_broadcast_peer(channel);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "broadcast peer add failed on ch=%u: %s",
                 channel, esp_err_to_name(err));
    }
}

static void set_espnow_channel(uint8_t channel)
{
    if (channel == 0 || channel > 11) return;
    esp_err_t err = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "channel %u failed: %s", channel, esp_err_to_name(err));
        return;
    }
    g_current_channel = channel;
    if (g_espnow_ready) {
        refresh_espnow_peers(channel);
    }
}

static void wifi_init_for_espnow(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wcfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    g_current_channel = initial_espnow_channel();
    ESP_ERROR_CHECK(esp_wifi_set_channel(g_current_channel, WIFI_SECOND_CHAN_NONE));
}

static void log_espnow_send_failure(const uint8_t *mac)
{
    if (mac) {
        ESP_LOGW(TAG, "espnow send failed to %02x:%02x:%02x:%02x:%02x:%02x",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        if (memcmp(mac, g_leader_mac, 6) == 0 && g_registered) {
            if (g_consecutive_send_failures < UINT8_MAX) {
                g_consecutive_send_failures++;
            }
            if (g_consecutive_send_failures >= SENSOR_SEND_FAILURES_BEFORE_DISCOVERY) {
                g_registered = 0;
                g_consecutive_send_failures = 0;
                ESP_LOGW(TAG, "leader send failed repeatedly; returning to channel discovery");
            }
        }
    } else {
        ESP_LOGW(TAG, "espnow send failed");
    }
}

static void record_espnow_send_success(const uint8_t *mac)
{
    if (!mac || memcmp(mac, g_leader_mac, 6) != 0) return;
    if (g_consecutive_send_failures > 0) {
        g_consecutive_send_failures--;
    }
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
static void on_espnow_send(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)
{
    if (status != ESP_NOW_SEND_SUCCESS) {
        log_espnow_send_failure(tx_info ? tx_info->des_addr : NULL);
    } else {
        record_espnow_send_success(tx_info ? tx_info->des_addr : NULL);
    }
}
#else
static void on_espnow_send(const uint8_t *mac, esp_now_send_status_t status)
{
    if (status != ESP_NOW_SEND_SUCCESS) {
        log_espnow_send_failure(mac);
    } else {
        record_espnow_send_success(mac);
    }
}
#endif

/* Forward declaration; defined below. */
static void send_register(void);

static void set_leader_from_mac(const uint8_t new_leader_mac[6])
{
    if (memcmp(g_leader_mac, new_leader_mac, 6) != 0) {
        esp_now_del_peer(g_leader_mac);
    }
    memcpy(g_leader_mac, new_leader_mac, 6);
    g_consecutive_send_failures = 0;

    add_leader_peer(g_current_channel);

    ESP_LOGI(TAG, "using leader %02x:%02x:%02x:%02x:%02x:%02x",
             g_leader_mac[0], g_leader_mac[1], g_leader_mac[2],
             g_leader_mac[3], g_leader_mac[4], g_leader_mac[5]);
}

static void switch_leader(const uint8_t new_leader_mac[6])
{
    /* The coordinator announcement tells us which C5 to use. Re-pair and
     * register again so telemetry resumes on the acting leader. */
    set_leader_from_mac(new_leader_mac);
    g_registered = 0;
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
            if (strncmp(ack->spot_id, g_spot_id, SPOT_ID_MAX_LEN) != 0) {
                ESP_LOGW(TAG, "ignoring register ack for %s", ack->spot_id);
                break;
            }
            if (ack->accepted) {
                if (g_registered && memcmp(hdr->sender_mac, g_leader_mac, 6) != 0) {
                    ESP_LOGI(TAG, "ignoring late register ack from %02x:%02x:%02x:%02x:%02x:%02x",
                             hdr->sender_mac[0], hdr->sender_mac[1], hdr->sender_mac[2],
                             hdr->sender_mac[3], hdr->sender_mac[4], hdr->sender_mac[5]);
                    break;
                }
                set_leader_from_mac(hdr->sender_mac);
                g_registered = 1;
                g_consecutive_send_failures = 0;
            }
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
    g_espnow_ready = 1;

    refresh_espnow_peers(g_current_channel);
    return ESP_OK;
}

static void send_register_to(const uint8_t dest_mac[6])
{
    Register msg = {0};
    msg.header.protocol_version = PROTOCOL_VERSION;
    msg.header.msg_type = MSG_REGISTER;
    memcpy(msg.header.sender_mac, g_my_mac, 6);
    msg.header.lamport_ts = lamport_tick();
    strncpy(msg.spot_id, g_spot_id, SPOT_ID_MAX_LEN - 1);
    msg.firmware_version = FIRMWARE_VERSION;
    esp_now_send(dest_mac, (const uint8_t *)&msg, sizeof(msg));
}

static void send_register(void)
{
    send_register_to(g_leader_mac);
    if (!g_registered) {
        send_register_to(BROADCAST_MAC);
    }
}

static void maybe_retry_register(uint32_t now_ms)
{
    if (g_registered) return;
    if ((uint32_t)(now_ms - g_last_register_ms) < REGISTER_RETRY_MS) return;

    set_espnow_channel(next_discovery_channel());
    ESP_LOGI(TAG, "register probe %s on ch=%u", g_spot_id, g_current_channel);
    send_register();
    g_last_register_ms = now_ms;
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
    for (int i = 0; i < SENSOR_UPDATE_SEND_ATTEMPTS; i++) {
        esp_now_send(g_leader_mac, (const uint8_t *)&msg, sizeof(msg));
        if (i + 1 < SENSOR_UPDATE_SEND_ATTEMPTS) {
            vTaskDelay(pdMS_TO_TICKS(SENSOR_UPDATE_SEND_RETRY_MS));
        }
    }
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

static int calibrate_distance(uint16_t raw_mm, uint16_t *out_mm)
{
    if (raw_mm == 0 || raw_mm >= DISTANCE_MAX_VALID_MM) return 0;

    int32_t adjusted = (int32_t)raw_mm + DISTANCE_OFFSET_MM;
    if (adjusted < DISTANCE_MIN_VALID_MM || adjusted >= DISTANCE_MAX_VALID_MM) return 0;

    *out_mm = (uint16_t)adjusted;
    return 1;
}

static uint16_t distance_delta(uint16_t a, uint16_t b)
{
    return a > b ? a - b : b - a;
}

static uint16_t median_distance(const uint16_t *values, uint8_t len)
{
    uint16_t sorted[DISTANCE_FILTER_WINDOW];
    memcpy(sorted, values, len * sizeof(uint16_t));

    for (uint8_t i = 1; i < len; i++) {
        uint16_t v = sorted[i];
        int j = (int)i - 1;
        while (j >= 0 && sorted[j] > v) {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = v;
    }

    return sorted[len / 2];
}

static int distance_filter_step(uint16_t raw_mm, uint16_t *out_mm)
{
    uint16_t calibrated_mm = 0;
    if (!calibrate_distance(raw_mm, &calibrated_mm)) return 0;

    if (g_stable_distance_mm == 0) {
        g_distance_window[g_distance_window_idx] = calibrated_mm;
        g_distance_window_idx = (g_distance_window_idx + 1) % DISTANCE_FILTER_WINDOW;
        if (g_distance_window_len < DISTANCE_FILTER_WINDOW) {
            g_distance_window_len++;
        }
        if (g_distance_window_len < DISTANCE_FILTER_WINDOW) return 0;

        g_stable_distance_mm = median_distance(g_distance_window, g_distance_window_len);
        *out_mm = g_stable_distance_mm;
        return 1;
    }

    if (distance_delta(calibrated_mm, g_stable_distance_mm) <= DISTANCE_JUMP_TOLERANCE_MM) {
        g_stable_distance_mm = calibrated_mm;
        g_candidate_distance_mm = 0;
        g_candidate_distance_count = 0;
        *out_mm = g_stable_distance_mm;
        return 1;
    }

    if (g_candidate_distance_mm == 0 ||
        distance_delta(calibrated_mm, g_candidate_distance_mm) > DISTANCE_JUMP_TOLERANCE_MM) {
        g_candidate_distance_mm = calibrated_mm;
        g_candidate_distance_count = 1;
    } else if (g_candidate_distance_count < 255) {
        g_candidate_distance_mm = (uint16_t)(((uint32_t)g_candidate_distance_mm + calibrated_mm) / 2);
        g_candidate_distance_count++;
    }

    if (g_candidate_distance_count >= DISTANCE_JUMP_CONFIRM_COUNT) {
        g_stable_distance_mm = g_candidate_distance_mm;
        g_candidate_distance_mm = 0;
        g_candidate_distance_count = 0;
    }

    *out_mm = g_stable_distance_mm;
    return 1;
}

static int distance_matches_state(SpotState state, uint16_t mm)
{
    if (mm == 0) return 0;
    if (state == SPOT_OCCUPIED) return mm <= OCCUPANCY_THRESHOLD_MM;
    if (state == SPOT_FREE) return mm > OCCUPANCY_THRESHOLD_MM;
    return 0;
}

static uint16_t report_distance_for_state(SpotState state, uint16_t mm)
{
    if (distance_matches_state(state, mm)) {
        g_last_reported_distance_mm = mm;
        return mm;
    }

    if (distance_matches_state(state, g_last_reported_distance_mm)) {
        return g_last_reported_distance_mm;
    }

    return state == SPOT_OCCUPIED
        ? OCCUPANCY_THRESHOLD_MM
        : OCCUPANCY_THRESHOLD_MM + 1;
}

/* ------------------------------------------------------------------------
 * Main task
 * ------------------------------------------------------------------------ */

static void sensor_task(void *arg)
{
    while (1) {
        uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
        maybe_retry_register(now_ms);

        uint16_t mm = 0;
        esp_err_t err = vl53l0x_read_range_mm(&mm);
        if (err != ESP_OK) {
            if ((uint32_t)(now_ms - g_last_range_error_log_ms) >= TELEMETRY_INTERVAL_MS) {
                ESP_LOGW(TAG, "range read failed: %s", esp_err_to_name(err));
                if (!g_vl53l0x_present) {
                    vl53l0x_detect(0);
                }
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

        uint16_t filtered_mm = 0;
        if (!distance_filter_step(mm, &filtered_mm)) {
            if ((uint32_t)(now_ms - g_last_range_error_log_ms) >= TELEMETRY_INTERVAL_MS) {
                ESP_LOGW(TAG, "range ignored as invalid/outlier raw=%u mm", mm);
                g_last_range_error_log_ms = now_ms;
            }
            vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
            continue;
        }

        uint8_t raw_occupied = (filtered_mm > 0 && filtered_mm <= OCCUPANCY_THRESHOLD_MM) ? 1 : 0;
        SpotState new_state = debounce_step(raw_occupied);
        uint16_t report_mm = report_distance_for_state(new_state, filtered_mm);

        if (new_state != SPOT_UNKNOWN &&
            (uint32_t)(now_ms - g_last_telemetry_ms) >= TELEMETRY_INTERVAL_MS) {
            if (new_state != g_last_state) {
                ESP_LOGI(TAG, "state change %d -> %d (%u mm filtered=%u raw=%u mm)",
                         g_last_state, new_state, report_mm, filtered_mm, mm);
            } else {
                ESP_LOGI(TAG, "telemetry refresh state=%d (%u mm filtered=%u raw=%u mm)",
                         new_state, report_mm, filtered_mm, mm);
            }
            g_last_state = new_state;
            send_sensor_update(new_state, report_mm);
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
    g_last_register_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "register probe %s on ch=%u", g_spot_id, g_current_channel);
    send_register();

    xTaskCreate(sensor_task, "sensor", 4096, NULL, 5, NULL);
}
