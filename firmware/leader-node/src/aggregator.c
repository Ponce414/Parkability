/*
 * aggregator.c — Option 2: ring buffer drained on WiFi windows.
 *
 * Inbound SensorUpdate frames go into a fixed ring (size AGG_RING_SIZE).
 * Telemetry is coalesced by spot_id, so a slow broker cannot make the
 * dashboard walk through stale distance readings.
 * aggregator_tick() drains the ring whenever the radio scheduler is in
 * WiFi mode AND wifi_uplink reports ready. If the ring fills up we drop
 * the oldest entry (FIFO eviction) and bump g_dropped_oldest — exposed
 * via aggregator_stats() for the concept-02 plots.
 *
 * Pushing also requests a WiFi switch from the radio scheduler. The
 * scheduler is free to refuse (it's bounded by ESPNOW_MAX_DWELL_MS) but
 * this is how we signal "drainable demand exists" without polling.
 *
 * Concurrency: ESP-NOW receive runs on the WiFi task; aggregator_tick is
 * driven from main loop. A FreeRTOS mutex protects the ring.
 *
 * Register handling: we cache the sender_mac from the incoming Register
 * header, add the sensor as an ESP-NOW peer, and reply with RegisterAck.
 * The reply happens immediately (we're already in ESP-NOW mode for the
 * inbound side); if the radio just switched to WiFi we still try — the
 * sensor will retry its Register on its own backoff.
 */

#include "aggregator.h"

#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "config.h"
#include "lamport.h"
#include "wifi_uplink.h"
#include "radio_scheduler.h"
#include "espnow_handler.h"

static const char *TAG = "agg";

#define AGG_RING_SIZE 32
/* Drain enough for the six-spot bring-up while keeping a small broker pause. */
#define AGG_DRAIN_BATCH 6
#define AGG_MIN_PUBLISH_INTERVAL_MS 100
#define AGG_RETRY_BACKOFF_MS 1000

static SensorUpdate g_ring[AGG_RING_SIZE];
static uint8_t g_head = 0;   /* next slot to write */
static uint8_t g_tail = 0;   /* next slot to read  */
static uint8_t g_count = 0;
static uint32_t g_dropped_oldest = 0;
static uint32_t g_published_ok = 0;
static uint32_t g_publish_failed = 0;
static uint32_t g_next_publish_attempt_ms = 0;

static SemaphoreHandle_t g_lock = NULL;
static uint8_t g_my_mac[6];
static char    g_my_mac_str[18];

static void format_mac(char out[18], const uint8_t mac[6])
{
    snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void aggregator_init(void)
{
    g_lock = xSemaphoreCreateMutex();
    g_head = g_tail = g_count = 0;
    esp_wifi_get_mac(WIFI_IF_STA, g_my_mac);
    format_mac(g_my_mac_str, g_my_mac);
    ESP_LOGI(TAG, "init mac=%s ring=%d", g_my_mac_str, AGG_RING_SIZE);
}

static int update_is_newer_or_equal(const SensorUpdate *next, const SensorUpdate *prev)
{
    if (next->header.lamport_ts != prev->header.lamport_ts) {
        return next->header.lamport_ts > prev->header.lamport_ts;
    }
    return next->wall_ts >= prev->wall_ts;
}

/* Push/update under lock. If full, evict oldest. */
static int ring_upsert_locked(const SensorUpdate *msg)
{
    for (uint8_t i = 0; i < g_count; i++) {
        uint8_t idx = (g_tail + i) % AGG_RING_SIZE;
        SensorUpdate *queued = &g_ring[idx];
        if (strncmp(queued->spot_id, msg->spot_id, SPOT_ID_MAX_LEN) == 0) {
            if (!update_is_newer_or_equal(msg, queued)) {
                return 0;
            }
            memcpy(queued, msg, sizeof(SensorUpdate));
            return 1;
        }
    }

    if (g_count == AGG_RING_SIZE) {
        /* Drop oldest: advance tail. */
        g_tail = (g_tail + 1) % AGG_RING_SIZE;
        g_count--;
        g_dropped_oldest++;
    }
    memcpy(&g_ring[g_head], msg, sizeof(SensorUpdate));
    g_head = (g_head + 1) % AGG_RING_SIZE;
    g_count++;
    return 1;
}

/* Pop under lock. Returns 1 on success, 0 if empty. */
static int ring_pop_locked(SensorUpdate *out)
{
    if (g_count == 0) return 0;
    memcpy(out, &g_ring[g_tail], sizeof(SensorUpdate));
    g_tail = (g_tail + 1) % AGG_RING_SIZE;
    g_count--;
    return 1;
}

void aggregator_on_sensor_update(const SensorUpdate *msg)
{
    xSemaphoreTake(g_lock, portMAX_DELAY);
    int queued = ring_upsert_locked(msg);
    int depth = g_count;
    xSemaphoreGive(g_lock);

    if (queued) {
        ESP_LOGI(TAG, "queued latest %s state=%u distance=%u (depth=%d)",
                 msg->spot_id, msg->state, (unsigned)msg->raw_distance_mm, depth);
    }

    /* Signal the scheduler that we have drainable demand. It may be in
     * an ESP-NOW dwell and refuse — that's fine, the dwell cap will
     * release it shortly. */
    if (queued) {
        radio_scheduler_request_switch(RADIO_MODE_WIFI);
    }
}

void aggregator_on_register(const Register *msg)
{
    const uint8_t *sensor_mac = msg->header.sender_mac;

    /* Add the sensor as an ESP-NOW peer so we can unicast the ack. Idempotent
     * at the ESP-NOW layer — if it's already there, this returns an error
     * we ignore. */
    espnow_handler_add_peer(sensor_mac);

    RegisterAck ack = {0};
    ack.header.protocol_version = PROTOCOL_VERSION;
    ack.header.msg_type = MSG_REGISTER_ACK;
    memcpy(ack.header.sender_mac, g_my_mac, 6);
    ack.header.lamport_ts = lamport_tick();
    strncpy(ack.spot_id, msg->spot_id, SPOT_ID_MAX_LEN - 1);
    ack.accepted = 1;

    int rc = espnow_handler_send(sensor_mac, (const uint8_t *)&ack, sizeof(ack));
    ESP_LOGI(TAG, "register spot=%s fw=%u ack_rc=%d",
             msg->spot_id, (unsigned)msg->firmware_version, rc);
}

void aggregator_tick(void)
{
    if (g_count == 0) return;
    if (radio_scheduler_current_mode() != RADIO_MODE_WIFI) return;
    if (!wifi_uplink_is_ready()) return;

    uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    if ((int32_t)(now_ms - g_next_publish_attempt_ms) < 0) return;

    for (int i = 0; i < AGG_DRAIN_BATCH; i++) {
        SensorUpdate msg;
        xSemaphoreTake(g_lock, portMAX_DELAY);
        int got = ring_pop_locked(&msg);
        xSemaphoreGive(g_lock);
        if (!got) break;

        int rc = wifi_uplink_publish_update(&msg, g_my_mac_str);
        g_next_publish_attempt_ms = now_ms + AGG_MIN_PUBLISH_INTERVAL_MS;
        if (rc == 0) {
            g_published_ok++;
        } else {
            g_next_publish_attempt_ms = now_ms + AGG_RETRY_BACKOFF_MS;
            /* Re-queue at the head so we don't lose it. We give up after
             * this tick; the next tick (still in WiFi mode, hopefully)
             * will retry. If WiFi window ends we go back to ESP-NOW and
             * the message waits there. */
            xSemaphoreTake(g_lock, portMAX_DELAY);
            if (g_count < AGG_RING_SIZE) {
                /* Push to the front: move tail back. */
                g_tail = (g_tail + AGG_RING_SIZE - 1) % AGG_RING_SIZE;
                memcpy(&g_ring[g_tail], &msg, sizeof(SensorUpdate));
                g_count++;
            } else {
                /* Concurrent pushes filled the ring while we were publishing.
                 * Drop the in-flight message rather than corrupt the ring. */
                g_dropped_oldest++;
            }
            xSemaphoreGive(g_lock);
            g_publish_failed++;
            break;
        }
    }
}
