/*
 * radio_scheduler.c — naive time-slicing stub.
 *
 * STATUS: stub. The interface is real; the policy is the team's choice.
 *
 * =========================================================================
 * TODO(team): evaluate strategies and pick one.
 *
 *   Strategy A — fixed time-slicing (CURRENT STUB)
 *     Alternate between ESP-NOW and WiFi every N ms. Simple, predictable.
 *     Downside: always pays a mode-switch cost even when idle; sensor updates
 *     stuck waiting out a WiFi window.
 *
 *   Strategy B — inactivity-based
 *     Stay in ESP-NOW mode until the aggregator says "I have a batch to
 *     upload" OR a ~max_wifi_gap timer fires. Better latency for sensor
 *     updates; harder to reason about backend propagation delay bounds.
 *
 *   Strategy C — pseudo dual-core (S3/C6 only, not C3)
 *     Pin ESP-NOW to core 0 and WiFi to core 1. Doesn't actually solve the
 *     single-radio problem on C3 — listed here so we remember to reconsider
 *     if we swap chips.
 *
 * Decide by: 2026-05-01 (before bring-up of second leader node).
 * Owner: embedded team. Measurement: see radio_scheduler_get_stats() and
 * the plots in analysis/.
 * =========================================================================
 */

#include "radio_scheduler.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "radio_sched";

#define ESPNOW_WINDOW_MS 100
#define WIFI_WINDOW_MS   100

static RadioMode  g_mode = RADIO_MODE_ESPNOW;
static RadioStats g_stats = {0};
static volatile int g_switch_requested = -1;  /* -1 = none, else target mode */

static void enter_mode(RadioMode mode)
{
    if (mode == g_mode) return;
    g_mode = mode;
    g_stats.mode_switches++;
    /* Real implementation would reconfigure WiFi STA association vs ESP-NOW
     * peer state here. For the stub we just flip the flag — the module
     * contract (current_mode, request_switch) is what callers depend on. */
    ESP_LOGI(TAG, "switched to mode %d", (int)mode);
}

static void scheduler_task(void *arg)
{
    while (1) {
        if (g_switch_requested >= 0) {
            enter_mode((RadioMode)g_switch_requested);
            g_switch_requested = -1;
        }

        if (g_mode == RADIO_MODE_ESPNOW) {
            g_stats.espnow_windows++;
            vTaskDelay(pdMS_TO_TICKS(ESPNOW_WINDOW_MS));
            enter_mode(RADIO_MODE_WIFI);
        } else {
            g_stats.wifi_windows++;
            vTaskDelay(pdMS_TO_TICKS(WIFI_WINDOW_MS));
            enter_mode(RADIO_MODE_ESPNOW);
        }
    }
}

void radio_scheduler_start(void)
{
    xTaskCreate(scheduler_task, "radio_sched", 3072, NULL, 6, NULL);
}

RadioMode radio_scheduler_current_mode(void)
{
    return g_mode;
}

void radio_scheduler_request_switch(RadioMode mode)
{
    g_switch_requested = (int)mode;
}

RadioStats radio_scheduler_get_stats(void)
{
    return g_stats;
}
