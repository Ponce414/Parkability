/*
 * wifi_uplink.c — WiFi connection + MQTT/REST publish.
 *
 * Connection lifecycle:
 *   wifi_uplink_init -> esp_wifi_set_config + esp_wifi_connect
 *   on WIFI_EVENT_STA_DISCONNECTED -> esp_wifi_connect (auto retry)
 *   on IP_EVENT_STA_GOT_IP -> mark transport ready, init MQTT (if MQTT mode)
 *
 * Publish path:
 *   - Caller (aggregator) calls wifi_uplink_publish_update
 *   - We check radio_scheduler_current_mode() — if not WiFi, return -1
 *     so the aggregator can hold the message
 *   - Build a small JSON payload
 *   - MQTT: esp_mqtt_client_publish to "parking/<lot>/<zone>/events"
 *   - REST: esp_http_client POST to /ingest/event
 */

#include "wifi_uplink.h"

#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_client.h"

#include "config.h"
#include "radio_scheduler.h"

#if WIFI_SECURITY_ENTERPRISE
#include "esp_eap_client.h"
#endif

#if defined(UPLINK_PROTOCOL_MQTT)
#include "mqtt_client.h"
#endif

static const char *TAG = "uplink";

static volatile int g_wifi_connected = 0;
static volatile int g_transport_ready = 0;
static int g_scan_logged = 0;

#if defined(UPLINK_PROTOCOL_MQTT)
static esp_mqtt_client_handle_t g_mqtt = NULL;
#endif

static void log_matching_aps_once(const char *ssid)
{
    if (g_scan_logged) return;
    g_scan_logged = 1;

    wifi_scan_config_t scan_cfg = { 0 };
    scan_cfg.ssid = (uint8_t *)ssid;
    scan_cfg.show_hidden = true;
    esp_err_t err = esp_wifi_scan_start(&scan_cfg, true);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "scan for ssid=%s failed: %s", ssid, esp_err_to_name(err));
        return;
    }

    uint16_t ap_count = 0;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_LOGI(TAG, "scan found %u AP(s) for ssid=%s", (unsigned)ap_count, ssid);
    uint16_t record_count = ap_count < 8 ? ap_count : 8;
    wifi_ap_record_t records[8] = {0};
    if (record_count == 0) return;

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&record_count, records));
    for (uint16_t i = 0; i < record_count; i++) {
        ESP_LOGI(TAG, "ap[%u] ch=%u rssi=%d auth=%d phy=%s%s%s",
                 (unsigned)i, (unsigned)records[i].primary, records[i].rssi,
                 records[i].authmode,
                 records[i].phy_11a ? "11a/" : "",
                 records[i].phy_11ac ? "11ac/" : "",
                 records[i].phy_11ax ? "11ax" : "");
    }
}

/* ----------------------------------------------------------------------
 * WiFi event handling — connect, auto-reconnect, mark ready on IP.
 * ---------------------------------------------------------------------- */

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *event =
            (const wifi_event_sta_disconnected_t *)data;
        ESP_LOGW(TAG, "wifi disconnected reason=%d, retrying",
                 event ? event->reason : -1);
        g_wifi_connected = 0;
        g_transport_ready = 0;
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = (const ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "wifi got IP " IPSTR, IP2STR(&event->ip_info.ip));
        g_wifi_connected = 1;
#if defined(UPLINK_PROTOCOL_MQTT)
        if (g_mqtt) {
            esp_mqtt_client_start(g_mqtt);
        }
#else
        /* REST has no persistent connection — mark ready as soon as we have IP. */
        g_transport_ready = 1;
#endif
    }
}

#if defined(UPLINK_PROTOCOL_MQTT)
static void on_mqtt_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    switch ((esp_mqtt_event_id_t)id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "mqtt connected");
        g_transport_ready = 1;
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "mqtt disconnected");
        g_transport_ready = 0;
        break;
    default:
        break;
    }
}
#endif

/* ----------------------------------------------------------------------
 * Init
 * ---------------------------------------------------------------------- */

void wifi_uplink_init(const char *ssid, const char *password)
{
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &on_wifi_event, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &on_wifi_event, NULL, NULL));

    wifi_config_t wcfg = { 0 };

    size_t ssid_len = strnlen(ssid, sizeof(wcfg.sta.ssid));
    memcpy(wcfg.sta.ssid, ssid, ssid_len);
    if (strlen(ssid) > sizeof(wcfg.sta.ssid)) {
        ESP_LOGW(TAG, "configured SSID is longer than 32 bytes; using first 32 bytes");
    }

#if WIFI_SECURITY_ENTERPRISE
    wcfg.sta.threshold.authmode = WIFI_AUTH_WPA2_ENTERPRISE;
    wcfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wcfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    wcfg.sta.channel = ESPNOW_CHANNEL;
#else
    strncpy((char *)wcfg.sta.password, password, sizeof(wcfg.sta.password) - 1);
    wcfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
#endif
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wcfg));

#if WIFI_SECURITY_ENTERPRISE
    if (strlen(WIFI_EAP_IDENTITY) == 0 || strlen(WIFI_EAP_USERNAME) == 0 ||
        strlen(WIFI_EAP_PASSWORD) == 0) {
        ESP_LOGE(TAG, "missing eduroam EAP identity/username/password in config.local.h");
        return;
    }
    ESP_ERROR_CHECK(esp_eap_client_set_identity(
        (const unsigned char *)WIFI_EAP_IDENTITY, strlen(WIFI_EAP_IDENTITY)));
    ESP_ERROR_CHECK(esp_eap_client_set_username(
        (const unsigned char *)WIFI_EAP_USERNAME, strlen(WIFI_EAP_USERNAME)));
    ESP_ERROR_CHECK(esp_eap_client_set_password(
        (const unsigned char *)WIFI_EAP_PASSWORD, strlen(WIFI_EAP_PASSWORD)));
#if WIFI_EAP_USE_DEFAULT_CERT_BUNDLE
    ESP_ERROR_CHECK(esp_eap_client_use_default_cert_bundle(true));
#endif
    if (strlen(WIFI_EAP_SERVER_DOMAIN) > 0) {
        ESP_ERROR_CHECK(esp_eap_client_set_domain_name(WIFI_EAP_SERVER_DOMAIN));
    }
    ESP_ERROR_CHECK(esp_eap_client_set_eap_methods(ESP_EAP_TYPE_PEAP));
    ESP_ERROR_CHECK(esp_wifi_sta_enterprise_enable());
    ESP_LOGI(TAG, "wifi enterprise configured: ssid=%s identity=%s", ssid, WIFI_EAP_IDENTITY);
#endif

    log_matching_aps_once(ssid);

    /* esp_wifi_start was already called in main.c (needed for ESP-NOW too).
     * We trigger connect explicitly in case STA_START already fired before
     * the handler was registered. */
    esp_wifi_connect();

#if defined(UPLINK_PROTOCOL_MQTT)
    char uri[64];
    snprintf(uri, sizeof(uri), "mqtt://%s:%d", MQTT_BROKER_HOST, MQTT_BROKER_PORT);
    esp_mqtt_client_config_t mcfg = { 0 };
    mcfg.broker.address.uri = uri;
    g_mqtt = esp_mqtt_client_init(&mcfg);
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(
        g_mqtt, ESP_EVENT_ANY_ID, on_mqtt_event, NULL));
    /* esp_mqtt_client_start is called once we have an IP. */
    ESP_LOGI(TAG, "uplink init MQTT %s", uri);
#else
    ESP_LOGI(TAG, "uplink init REST http://%s:%d", REST_BACKEND_HOST, REST_BACKEND_PORT);
#endif
}

int wifi_uplink_is_ready(void)
{
    return g_wifi_connected && g_transport_ready;
}

/* ----------------------------------------------------------------------
 * Publish
 *
 * JSON shape matches protocols/leader-backend-api.md. We hand-roll the
 * JSON to avoid pulling in cJSON for two simple messages.
 * ---------------------------------------------------------------------- */

static const char *state_str(uint8_t s)
{
    switch (s) {
    case 1: return "FREE";
    case 2: return "OCCUPIED";
    default: return "UNKNOWN";
    }
}

#if defined(UPLINK_PROTOCOL_REST)
static int rest_post(const char *path, const char *json)
{
    char url[96];
    snprintf(url, sizeof(url), "http://%s:%d%s",
             REST_BACKEND_HOST, REST_BACKEND_PORT, path);

    esp_http_client_config_t cfg = { 0 };
    cfg.url = url;
    cfg.method = HTTP_METHOD_POST;
    cfg.timeout_ms = 2000;
    esp_http_client_handle_t cli = esp_http_client_init(&cfg);
    esp_http_client_set_header(cli, "Content-Type", "application/json");
    esp_http_client_set_post_field(cli, json, (int)strlen(json));

    esp_err_t err = esp_http_client_perform(cli);
    int status = esp_http_client_get_status_code(cli);
    esp_http_client_cleanup(cli);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "rest %s err=%d", path, err);
        return -1;
    }
    return (status >= 200 && status < 300) ? 0 : -1;
}
#endif

int wifi_uplink_publish_update(const SensorUpdate *update,
                               const char *leader_mac_str)
{
    if (!wifi_uplink_is_ready()) return -1;
    if (radio_scheduler_current_mode() != RADIO_MODE_WIFI) return -1;

    char json[256];
    snprintf(json, sizeof(json),
        "{\"spot_id\":\"%s\",\"zone_id\":\"%s\",\"state\":\"%s\","
        "\"lamport_ts\":%u,\"wall_ts\":%u,\"raw_distance_mm\":%u,"
        "\"leader_mac\":\"%s\"}",
        update->spot_id, ZONE_ID, state_str(update->state),
        (unsigned)update->header.lamport_ts, (unsigned)update->wall_ts,
        (unsigned)update->raw_distance_mm, leader_mac_str);

#if defined(UPLINK_PROTOCOL_MQTT)
    char topic[64];
    snprintf(topic, sizeof(topic), "parking/%s/%s/events", ZONE_LOT, ZONE_ID);
    int id = esp_mqtt_client_publish(g_mqtt, topic, json, 0, 1, 0);
    return (id >= 0) ? 0 : -1;
#else
    return rest_post("/ingest/event", json);
#endif
}

int wifi_uplink_publish_leader_change(const char *zone_id,
                                      const char *new_leader_mac)
{
    if (!wifi_uplink_is_ready()) return -1;

    char json[128];
    snprintf(json, sizeof(json),
        "{\"zone_id\":\"%s\",\"leader_mac\":\"%s\"}",
        zone_id, new_leader_mac);

#if defined(UPLINK_PROTOCOL_MQTT)
    char topic[64];
    snprintf(topic, sizeof(topic), "parking/%s/%s/leader", ZONE_LOT, zone_id);
    int id = esp_mqtt_client_publish(g_mqtt, topic, json, 0, 1, 1 /* retain */);
    return (id >= 0) ? 0 : -1;
#else
    return rest_post("/ingest/leader", json);
#endif
}
