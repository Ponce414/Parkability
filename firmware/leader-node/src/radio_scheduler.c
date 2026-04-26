/*
 * radio_scheduler.c — inactivity-based radio mode arbiter (Strategy B).
 *
 * Single-radio C3 chips can't run ESP-NOW and connected WiFi STA truly
 * concurrently. This module owns the policy for switching between them.
 *
 * Strategy B (chosen):
 *   - Default to ESP-NOW. Sensor traffic is the steady state.
 *   - The aggregator calls radio_scheduler_request_switch(RADIO_MODE_WIFI)
 *     when its ring has data ready to drain. We honor the switch.
 *   - Cap each ESP-NOW dwell at ESPNOW_MAX_DWELL_MS so a fully idle aggregator
 *     still gets a periodic WiFi window (lets the broker keep our session,
 *     and gives REST a chance even if no events arrive).
 *   - Cap each WiFi dwell at WIFI_MAX_DWELL_MS so we always come back to
 *     ESP-NOW promptly — sensor updates are the latency-critical path.
 *
 * Why this is safer than fixed time-slicing: sensor traffic isn't blocked
 * waiting for an arbitrary WiFi window. Why it's safer than "stay in WiFi
 * forever once connected": ESP-NOW frames sent while in WiFi mode silently
 * drop if the channel doesn't match the AP — measured as
 * espnow_dropped_due_to_mode for concept 02 plots.
 */

#include "radio_scheduler.h"

#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "config.h"

static const char *TAG = "radio_sched";

static RadioMode  g_mode = RADIO_MODE_ESPNOW;
static RadioStats g_stats = {0};
static volatile int g_switch_requested = -1;  /* -1 = none, else target mode */
static int64_t g_mode_entered_us = 0;

static void enter_mode(RadioMode mode)
{
    if (mode == g_mode) return;
    g_mode = mode;
    g_mode_entered_us = esp_timer_get_time();
    g_stats.mode_switches++;
    if (mode == RADIO_MODE_ESPNOW) g_stats.espnow_windows++;
    else                            g_stats.wifi_windows++;
    /* On a real C3 deployment we'd call esp_wifi_set_ps(WIFI_PS_NONE) +
     * channel realignment here. The ESP-IDF WiFi+ESP-NOW stack handles
     * the underlying channel arbitration; the value of this module is
     * giving the rest of the firmware a single boolean to gate sends on. */
    ESP_LOGI(TAG, "mode -> %s",
             mode == RADIO_MODE_ESPNOW ? "ESP-NOW" : "WIFI");
}

static int64_t ms_in_current_mode(void)
{
    return (esp_timer_get_time() - g_mode_entered_us) / 1000;
}

static void scheduler_task(void *arg)
{
    g_mode_entered_us = esp_timer_get_time();
    while (1) {
        /* Honor any explicit switch request first. */
        int req = g_switch_requested;
        if (req >= 0) {
            g_switch_requested = -1;
            enter_mode((RadioMode)req);
        }

        /* Enforce the dwell cap for the current mode. */
        int64_t elapsed_ms = ms_in_current_mode();
        if (g_mode == RADIO_MODE_ESPNOW && elapsed_ms >= ESPNOW_MAX_DWELL_MS) {
            enter_mode(RADIO_MODE_WIFI);
        } else if (g_mode == RADIO_MODE_WIFI && elapsed_ms >= WIFI_MAX_DWELL_MS) {
            enter_mode(RADIO_MODE_ESPNOW);
        }

        /* Poll cadence. Short enough to be responsive to switch requests
         * without busy-waiting. */
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void radio_scheduler_start(void)
{
    g_mode = RADIO_MODE_ESPNOW;
    g_mode_entered_us = esp_timer_get_time();
    xTaskCreate(scheduler_task, "radio_sched", 3072, NULL, 6, NULL);
    ESP_LOGI(TAG, "started: espnow_dwell=%d ms, wifi_dwell=%d ms",
             ESPNOW_MAX_DWELL_MS, WIFI_MAX_DWELL_MS);
}

RadioMode radio_scheduler_current_mode(void)
{
    return g_mode;
}

void radio_scheduler_request_switch(RadioMode mode)
{
    /* If we're already in the target mode, reset the dwell timer so we don't
     * snap back early — the caller is signalling fresh demand. */
    if (mode == g_mode) {
        g_mode_entered_us = esp_timer_get_time();
        return;
    }
    g_switch_requested = (int)mode;
}

RadioStats radio_scheduler_get_stats(void)
{
    return g_stats;
}
