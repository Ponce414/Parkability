/*
 * aggregator.c
 *
 * STATUS: interface real, batching policy TODO.
 *
 * =========================================================================
 * TODO(software team): pick a batching/throttling policy.
 *
 *   Option 1 — immediate forward
 *     As soon as an update arrives, try to publish. Drops if radio is in
 *     ESP-NOW mode. Simple, lowest latency when radio cooperates; worst
 *     drop rate during ESP-NOW windows.
 *
 *   Option 2 — ring buffer, drain on WiFi window
 *     Push every update into a small ring (e.g., 32 entries). aggregator_tick
 *     drains what it can when radio_scheduler_current_mode() == WIFI.
 *     Bounded memory, handles bursts cleanly, needs an eviction policy when
 *     full (recommend: drop oldest, increment a stat).
 *
 *   Option 3 — dedupe + coalesce
 *     Keep only the *latest* state per spot_id in a map. Reduces load when
 *     a spot toggles quickly. Adds logic complexity.
 *
 *   Recommended start: Option 2 with ring size 32. Revisit after measuring.
 * =========================================================================
 */

#include "aggregator.h"

#include <string.h>
#include "esp_log.h"

#include "wifi_uplink.h"
#include "radio_scheduler.h"

static const char *TAG = "agg";

/* Placeholder ring buffer skeleton — see Option 2 above. */
#define AGG_RING_SIZE 32
static SensorUpdate g_ring[AGG_RING_SIZE];
static uint8_t g_ring_head = 0;
static uint8_t g_ring_tail = 0;
static uint8_t g_ring_count = 0;

void aggregator_init(void)
{
    g_ring_head = g_ring_tail = g_ring_count = 0;
}

void aggregator_on_sensor_update(const SensorUpdate *msg)
{
    ESP_LOGI(TAG, "update %s state=%u", msg->spot_id, msg->state);
    /* TODO: actually enqueue and drain. For v1 we log and attempt immediate
     * forward (Option 1). Switch to Option 2 when testing shows drops. */
    wifi_uplink_publish_update(msg, "self");
}

void aggregator_on_register(const Register *msg)
{
    ESP_LOGI(TAG, "register spot=%s fw=%u", msg->spot_id, msg->firmware_version);
    /* TODO(team): send RegisterAck back. Need to cache sender_mac from the
     * incoming header so we can reply to the right peer. */
}

void aggregator_tick(void)
{
    /* No-op until the ring buffer is implemented. Kept so main_loop can
     * call it without a compile-time flag. */
    if (g_ring_count == 0) return;
    if (radio_scheduler_current_mode() != RADIO_MODE_WIFI) return;
    /* TODO: pop from ring, call wifi_uplink_publish_update until empty or
     * until the WiFi window ends. */
}
