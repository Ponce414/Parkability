/*
 * heartbeat.c — periodic heartbeat sender + per-peer timeout tracker.
 *
 * Sends heartbeats every HEARTBEAT_INTERVAL_MS to the broadcast MAC.
 * Maintains a peer table from incoming heartbeats. The main loop polls
 * heartbeat_check_timeout() which fires bully when the *coordinator*
 * (specifically) has been silent past HEARTBEAT_TIMEOUT_MS.
 */

#include "heartbeat.h"

#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "config.h"
#include "lamport.h"
#include "espnow_handler.h"
#include "bully.h"

static const char *TAG = "hb";

static uint32_t g_my_priority = 0;
static uint8_t  g_my_mac[6];

typedef struct {
    int      used;
    uint8_t  mac[6];
    uint32_t priority;
    int64_t  last_heard_us;
} PeerEntry;

static PeerEntry        g_peers[MAX_PEER_TABLE];
static SemaphoreHandle_t g_peer_lock = NULL;

static uint8_t g_coordinator_mac[6] = {0};
static uint32_t g_coordinator_priority = 0;
static int      g_coordinator_known = 0;
static int      g_outage_already_fired = 0;

static const uint8_t BROADCAST_MAC[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

static int find_peer_locked(const uint8_t mac[6])
{
    for (int i = 0; i < MAX_PEER_TABLE; i++) {
        if (g_peers[i].used && memcmp(g_peers[i].mac, mac, 6) == 0) return i;
    }
    return -1;
}

static int alloc_peer_locked(void)
{
    for (int i = 0; i < MAX_PEER_TABLE; i++) {
        if (!g_peers[i].used) return i;
    }
    return -1;
}

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
    if (g_peer_lock == NULL) g_peer_lock = xSemaphoreCreateMutex();
    memset(g_peers, 0, sizeof(g_peers));
    /* Add broadcast MAC to ESP-NOW peer table so heartbeats can be sent. */
    espnow_handler_add_peer(BROADCAST_MAC);
    xTaskCreate(heartbeat_task, "hb", 3072, NULL, 5, NULL);
}

void heartbeat_on_received(const Heartbeat *msg)
{
    const uint8_t *mac = msg->header.sender_mac;
    if (memcmp(mac, g_my_mac, 6) == 0) return;   /* don't track ourselves */

    xSemaphoreTake(g_peer_lock, portMAX_DELAY);
    int idx = find_peer_locked(mac);
    if (idx < 0) {
        idx = alloc_peer_locked();
        if (idx < 0) {
            xSemaphoreGive(g_peer_lock);
            ESP_LOGW(TAG, "peer table full, ignoring new peer");
            return;
        }
        g_peers[idx].used = 1;
        memcpy(g_peers[idx].mac, mac, 6);
        /* Add to ESP-NOW peer table so we can unicast OK / COORDINATOR. */
        espnow_handler_add_peer(mac);
        ESP_LOGI(TAG, "new peer 0x%08x", (unsigned)msg->node_priority);
    }
    g_peers[idx].priority = msg->node_priority;
    g_peers[idx].last_heard_us = esp_timer_get_time();

    /* If the sender claims to be the current leader, adopt them as
     * coordinator unless they're lower priority than the one we already
     * know about (the bully algorithm should reconcile this on its own). */
    if (msg->is_current_leader) {
        if (!g_coordinator_known || msg->node_priority >= g_coordinator_priority) {
            memcpy(g_coordinator_mac, mac, 6);
            g_coordinator_priority = msg->node_priority;
            g_coordinator_known = 1;
            g_outage_already_fired = 0;
        }
    }
    xSemaphoreGive(g_peer_lock);
}

int heartbeat_check_timeout(void)
{
    /* If we are the coordinator, nobody can time out our heartbeats — return. */
    if (bully_current_state() == BULLY_COORDINATOR) return 0;

    /* If we don't know who the coordinator is yet, kick an election so we
     * find one. (Happens at boot before any heartbeat has been heard.) */
    if (!g_coordinator_known) {
        if (!g_outage_already_fired) {
            g_outage_already_fired = 1;
            ESP_LOGW(TAG, "no coordinator known, triggering election");
            bully_on_heartbeat_timeout();
            return 1;
        }
        return 0;
    }

    int64_t now_us = esp_timer_get_time();
    int fired = 0;
    xSemaphoreTake(g_peer_lock, portMAX_DELAY);
    int idx = find_peer_locked(g_coordinator_mac);
    if (idx < 0) {
        /* We adopted them but never logged a heartbeat — count as silent. */
        if (!g_outage_already_fired) {
            g_outage_already_fired = 1;
            fired = 1;
        }
    } else {
        int64_t gap_ms = (now_us - g_peers[idx].last_heard_us) / 1000;
        if (gap_ms > HEARTBEAT_TIMEOUT_MS && !g_outage_already_fired) {
            ESP_LOGW(TAG, "coordinator silent %lld ms", (long long)gap_ms);
            g_outage_already_fired = 1;
            fired = 1;
        }
    }
    xSemaphoreGive(g_peer_lock);

    if (fired) bully_on_heartbeat_timeout();
    return fired;
}

void heartbeat_set_coordinator(const uint8_t coordinator_mac[6], uint32_t priority)
{
    xSemaphoreTake(g_peer_lock, portMAX_DELAY);
    memcpy(g_coordinator_mac, coordinator_mac, 6);
    g_coordinator_priority = priority;
    g_coordinator_known = 1;
    g_outage_already_fired = 0;
    /* Also reset last-heard so the new coordinator gets a fair grace window. */
    int idx = find_peer_locked(coordinator_mac);
    if (idx >= 0) {
        g_peers[idx].last_heard_us = esp_timer_get_time();
    }
    xSemaphoreGive(g_peer_lock);
}

int heartbeat_get_higher_priority_peers(uint8_t out_macs[][6], int max)
{
    int n = 0;
    xSemaphoreTake(g_peer_lock, portMAX_DELAY);
    for (int i = 0; i < MAX_PEER_TABLE && n < max; i++) {
        if (g_peers[i].used && g_peers[i].priority > g_my_priority) {
            memcpy(out_macs[n++], g_peers[i].mac, 6);
        }
    }
    xSemaphoreGive(g_peer_lock);
    return n;
}

int heartbeat_get_peer_mac(uint32_t priority, uint8_t out_mac[6])
{
    int found = -1;
    xSemaphoreTake(g_peer_lock, portMAX_DELAY);
    for (int i = 0; i < MAX_PEER_TABLE; i++) {
        if (g_peers[i].used && g_peers[i].priority == priority) {
            memcpy(out_mac, g_peers[i].mac, 6);
            found = 0;
            break;
        }
    }
    xSemaphoreGive(g_peer_lock);
    return found;
}
