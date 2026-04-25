/*
 * heartbeat.c — periodic heartbeat sender (real) + timeout detector (stub).
 *
 * The send side is trivially correct and is fully implemented. The receive
 * side (tracking last-heard-from per peer and detecting a timeout streak)
 * is left as a specified stub because the exact data structure depends on
 * the team's choice of peer registry (static array vs. list).
 */

#include "heartbeat.h"

#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lamport.h"
#include "espnow_handler.h"
#include "bully.h"

static const char *TAG = "heartbeat";

static uint32_t g_my_priority = 0;
static uint8_t  g_my_mac[6];

/* Broadcast MAC — ESP-NOW treats this as "send to everyone registered as
 * a peer", which is what we want for heartbeats in a small zone. */
static const uint8_t BROADCAST_MAC[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

static int64_t g_last_peer_heartbeat_us = 0;

static void heartbeat_task(void *arg)
{
    while (1) {
        Heartbeat msg = {0};
        msg.header.protocol_version = PROTOCOL_VERSION;
        msg.header.msg_type = MSG_HEARTBEAT;
        memcpy(msg.header.sender_mac, g_my_mac, 6);
        msg.header.lamport_ts = lamport_tick();
        msg.node_priority = g_my_priority;
        msg.is_current_leader = (bully_current_state() == BULLY_COORDINATOR) ? 1 : 0;

        espnow_handler_send(BROADCAST_MAC, (const uint8_t *)&msg, sizeof(msg));
        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS));
    }
}

void heartbeat_init(uint32_t my_priority)
{
    g_my_priority = my_priority;
    esp_wifi_get_mac(WIFI_IF_STA, g_my_mac);
    g_last_peer_heartbeat_us = esp_timer_get_time();
    xTaskCreate(heartbeat_task, "hb", 3072, NULL, 5, NULL);
}

void heartbeat_on_received(const Heartbeat *msg)
{
    /* TODO(team): track last-heard per peer in a small table so we can
     * detect "current leader stopped heartbeating" vs "some random peer
     * is quiet". The v1 stub treats any peer's heartbeat as liveness. */
    g_last_peer_heartbeat_us = esp_timer_get_time();
    (void)msg;
}

int heartbeat_check_timeout(void)
{
    /* TODO(team): the real check should track the current leader specifically,
     * not any peer. Current behavior: fire timeout if nobody at all has
     * heartbeated in HEARTBEAT_TIMEOUT_MS. Fine for initial bring-up with
     * just two nodes. */
    int64_t now_us = esp_timer_get_time();
    int64_t gap_ms = (now_us - g_last_peer_heartbeat_us) / 1000;
    if (gap_ms > HEARTBEAT_TIMEOUT_MS) {
        ESP_LOGW(TAG, "heartbeat timeout: %lld ms since last peer heartbeat",
                 (long long)gap_ms);
        g_last_peer_heartbeat_us = now_us;  /* reset so we don't spam bully */
        bully_on_heartbeat_timeout();
        return 1;
    }
    return 0;
}
