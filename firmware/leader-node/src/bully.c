/*
 * bully.c — Bully algorithm for leader election.
 *
 * State machine documented in bully.h. Timers (T_OK, T_COORD) are esp_timer
 * one-shots that fire callbacks back into the state machine.
 *
 * All ESP-NOW sends here go via the broadcast MAC for COORDINATOR
 * announcements (so sensors can re-pair) and to specific peer MACs for
 * ELECTION/OK (looked up via heartbeat's peer table).
 */

#include "bully.h"

#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"

#include "config.h"
#include "lamport.h"
#include "espnow_handler.h"
#include "heartbeat.h"
#include "wifi_uplink.h"

static const char *TAG = "bully";

static BullyState g_state = BULLY_IDLE;
static uint32_t   g_my_priority = 0;
static uint8_t    g_my_mac[6];

static esp_timer_handle_t g_t_ok = NULL;
static esp_timer_handle_t g_t_coord = NULL;

static const uint8_t BROADCAST_MAC[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

static void format_mac_str(char out[18], const uint8_t mac[6])
{
    snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

/* ----------------------------------------------------------------------
 * Send helpers
 * ---------------------------------------------------------------------- */

static void fill_header(MessageHeader *h, MessageType type)
{
    h->protocol_version = PROTOCOL_VERSION;
    h->msg_type = (uint8_t)type;
    memcpy(h->sender_mac, g_my_mac, 6);
    h->lamport_ts = lamport_tick();
}

static void send_election_to(const uint8_t peer[6])
{
    Election e = {0};
    fill_header(&e.header, MSG_ELECTION);
    e.candidate_priority = g_my_priority;
    espnow_handler_send(peer, (const uint8_t *)&e, sizeof(e));
}

static void send_ok_to(const uint8_t peer[6])
{
    OkMessage o = {0};
    fill_header(&o.header, MSG_OK);
    o.responder_priority = g_my_priority;
    espnow_handler_send(peer, (const uint8_t *)&o, sizeof(o));
}

static void broadcast_coordinator(void)
{
    Coordinator c = {0};
    fill_header(&c.header, MSG_COORDINATOR);
    c.coordinator_priority = g_my_priority;
    espnow_handler_send(BROADCAST_MAC, (const uint8_t *)&c, sizeof(c));
}

/* ----------------------------------------------------------------------
 * Becoming the coordinator
 * ---------------------------------------------------------------------- */

static void declare_self_coordinator(void)
{
    g_state = BULLY_COORDINATOR;
    heartbeat_set_coordinator(g_my_mac, g_my_priority);
    broadcast_coordinator();

    char mac_s[18];
    format_mac_str(mac_s, g_my_mac);
    wifi_uplink_publish_leader_change(ZONE_ID, mac_s);
    ESP_LOGI(TAG, "declared self coordinator (priority 0x%08x)", (unsigned)g_my_priority);
}

/* ----------------------------------------------------------------------
 * Timer callbacks
 * ---------------------------------------------------------------------- */

static void on_t_ok_expired(void *arg)
{
    if (g_state != BULLY_WAITING_FOR_OK) return;
    /* Nobody outranked us — we win. */
    ESP_LOGI(TAG, "T_OK expired, no higher-priority peer responded");
    declare_self_coordinator();
}

static void on_t_coord_expired(void *arg)
{
    if (g_state != BULLY_ELECTION_IN_PROGRESS) return;
    /* The peer that sent OK never followed up with COORDINATOR — restart. */
    ESP_LOGW(TAG, "T_COORD expired, restarting election");
    bully_on_heartbeat_timeout();
}

static void start_timer(esp_timer_handle_t t, int ms)
{
    esp_timer_stop(t);  /* harmless if not running */
    esp_timer_start_once(t, (uint64_t)ms * 1000);
}

/* ----------------------------------------------------------------------
 * Init
 * ---------------------------------------------------------------------- */

void bully_init(uint32_t my_priority)
{
    g_my_priority = my_priority;
    esp_wifi_get_mac(WIFI_IF_STA, g_my_mac);
    g_state = BULLY_IDLE;

    esp_timer_create_args_t a_ok = { .callback = &on_t_ok_expired,    .name = "t_ok" };
    esp_timer_create_args_t a_co = { .callback = &on_t_coord_expired, .name = "t_co" };
    esp_timer_create(&a_ok, &g_t_ok);
    esp_timer_create(&a_co, &g_t_coord);

    ESP_LOGI(TAG, "bully init priority=0x%08x state=IDLE", (unsigned)my_priority);
}

BullyState bully_current_state(void) { return g_state; }

/* ----------------------------------------------------------------------
 * Event handlers
 * ---------------------------------------------------------------------- */

void bully_on_heartbeat_timeout(void)
{
    if (g_state == BULLY_COORDINATOR) return;

    /* Find peers with strictly higher priority and send each an ELECTION. */
    uint8_t macs[MAX_PEER_TABLE][6];
    int n = heartbeat_get_higher_priority_peers(macs, MAX_PEER_TABLE);

    if (n == 0) {
        /* Nobody outranks us. Win immediately. */
        declare_self_coordinator();
        return;
    }

    for (int i = 0; i < n; i++) send_election_to(macs[i]);
    g_state = BULLY_WAITING_FOR_OK;
    start_timer(g_t_ok, BULLY_T_OK_MS);
    ESP_LOGI(TAG, "sent ELECTION to %d higher-priority peers", n);
}

void bully_on_election_received(const Election *msg)
{
    /* Anyone who sends us an ELECTION must be lower priority (otherwise
     * they wouldn't be sending — they'd just become coordinator). Reply
     * OK and start our own election. */
    if (msg->candidate_priority < g_my_priority) {
        send_ok_to(msg->header.sender_mac);
        bully_on_heartbeat_timeout();
    }
    /* If their priority >= ours, just ignore — they'll either win or get
     * outranked by someone else. */
}

void bully_on_ok_received(const OkMessage *msg)
{
    if (g_state != BULLY_WAITING_FOR_OK) return;
    /* Someone outranks us; stand down and wait for their COORDINATOR msg. */
    esp_timer_stop(g_t_ok);
    g_state = BULLY_ELECTION_IN_PROGRESS;
    start_timer(g_t_coord, BULLY_T_COORD_MS);
    ESP_LOGI(TAG, "received OK from priority=0x%08x; waiting for COORDINATOR",
             (unsigned)msg->responder_priority);
}

void bully_on_coordinator_received(const Coordinator *msg)
{
    /* Adopt the announced coordinator regardless of our state. The bully
     * algorithm treats COORDINATOR as authoritative. */
    esp_timer_stop(g_t_ok);
    esp_timer_stop(g_t_coord);
    heartbeat_set_coordinator(msg->header.sender_mac, msg->coordinator_priority);
    g_state = BULLY_IDLE;
    ESP_LOGI(TAG, "adopted coordinator priority=0x%08x",
             (unsigned)msg->coordinator_priority);
}
