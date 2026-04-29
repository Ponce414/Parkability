#include "espnow_handler.h"

#include <string.h>
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_idf_version.h"

#include "messages.h"
#include "lamport.h"
#include "aggregator.h"
#include "bully.h"
#include "heartbeat.h"

static const char *TAG = "espnow";

/* NOTE on zone size:
 * ESP-NOW allows up to ~20 encrypted peers (or ~10 on older SoCs). With one
 * backup leader per zone we reserve 1 peer slot for election traffic, leaving
 * ~19 for sensors. That's the reason zones cap out around 19 parking spots.
 * See concepts/08-scalability/. */
#define MAX_ESPNOW_PEERS 20

static void on_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    if (len < (int)sizeof(MessageHeader)) return;
    const MessageHeader *hdr = (const MessageHeader *)data;
    if (hdr->protocol_version != PROTOCOL_VERSION) {
        ESP_LOGW(TAG, "protocol version mismatch: got %u expected %u",
                 hdr->protocol_version, PROTOCOL_VERSION);
        return;
    }
    lamport_update(hdr->lamport_ts);

    switch (hdr->msg_type) {
    case MSG_SENSOR_UPDATE:
        if (len >= (int)sizeof(SensorUpdate)) {
            aggregator_on_sensor_update((const SensorUpdate *)data);
        }
        break;
    case MSG_REGISTER:
        if (len >= (int)sizeof(Register)) {
            aggregator_on_register((const Register *)data);
        }
        break;
    case MSG_HEARTBEAT:
        if (len >= (int)sizeof(Heartbeat)) {
            heartbeat_on_received((const Heartbeat *)data);
        }
        break;
    case MSG_ELECTION:
        if (len >= (int)sizeof(Election)) {
            bully_on_election_received((const Election *)data);
        }
        break;
    case MSG_OK:
        if (len >= (int)sizeof(OkMessage)) {
            bully_on_ok_received((const OkMessage *)data);
        }
        break;
    case MSG_COORDINATOR:
        if (len >= (int)sizeof(Coordinator)) {
            bully_on_coordinator_received((const Coordinator *)data);
        }
        break;
    default:
        ESP_LOGW(TAG, "unknown msg_type %u", hdr->msg_type);
        break;
    }
}

static void log_send_failure(const uint8_t *mac)
{
    if (mac) {
        ESP_LOGW(TAG, "send failed to %02x:%02x:%02x:%02x:%02x:%02x",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    } else {
        ESP_LOGW(TAG, "send failed");
    }
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
static void on_send(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)
{
    if (status != ESP_NOW_SEND_SUCCESS) {
        log_send_failure(tx_info ? tx_info->des_addr : NULL);
    }
}
#else
static void on_send(const uint8_t *mac, esp_now_send_status_t status)
{
    if (status != ESP_NOW_SEND_SUCCESS) {
        log_send_failure(mac);
    }
}
#endif

void espnow_handler_init(void)
{
    ESP_ERROR_CHECK(esp_now_init());
    esp_now_register_recv_cb(on_recv);
    esp_now_register_send_cb(on_send);
}

int espnow_handler_send(const uint8_t peer_mac[6], const uint8_t *data, uint16_t len)
{
    esp_err_t err = esp_now_send(peer_mac, data, len);
    return (err == ESP_OK) ? 0 : -1;
}

int espnow_handler_add_peer(const uint8_t peer_mac[6])
{
    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, peer_mac, 6);
    peer.ifidx   = WIFI_IF_STA;
    peer.encrypt = false;
    esp_err_t err = esp_now_add_peer(&peer);
    return (err == ESP_OK) ? 0 : -1;
}
