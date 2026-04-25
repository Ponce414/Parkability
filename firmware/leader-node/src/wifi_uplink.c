/*
 * wifi_uplink.c — stub implementation.
 *
 * The real wiring depends on which transport we pick (MQTT broker address &
 * topic vs REST endpoint path). Both are left as TODOs below with the exact
 * interfaces they should land against.
 */

#include "wifi_uplink.h"

#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_wifi.h"

#include "radio_scheduler.h"

static const char *TAG = "uplink";

static char g_ssid[33];
static char g_password[65];
static char g_backend_host[64];
static uint16_t g_backend_port;

void wifi_uplink_init(const char *ssid, const char *password,
                      const char *backend_host, uint16_t backend_port)
{
    strncpy(g_ssid,         ssid,         sizeof(g_ssid) - 1);
    strncpy(g_password,     password,     sizeof(g_password) - 1);
    strncpy(g_backend_host, backend_host, sizeof(g_backend_host) - 1);
    g_backend_port = backend_port;

    /* TODO(embedded team):
     *   - esp_wifi_set_config(WIFI_IF_STA, ...) with g_ssid / g_password
     *   - esp_wifi_connect()
     *   - if UPLINK_PROTOCOL_MQTT: init PubSubClient against g_backend_host:g_backend_port
     *     on topic "parking/<lot_id>/<zone_id>/events"
     *   - if UPLINK_PROTOCOL_REST: nothing to init; publish uses POST /ingest
     */
    ESP_LOGI(TAG, "uplink init ssid=%s backend=%s:%u", g_ssid, g_backend_host, g_backend_port);
}

int wifi_uplink_publish_update(const SensorUpdate *update, const char *leader_mac_str)
{
    if (radio_scheduler_current_mode() != RADIO_MODE_WIFI) {
        /* Can't transmit on WiFi right now; drop and let the aggregator decide
         * whether to buffer. Policy TBD — see aggregator.c TODO. */
        return -1;
    }

    /* TODO(embedded team): serialize to the wire format described in
     * protocols/leader-backend-api.md and publish via chosen transport.
     * For now, just log. */
    ESP_LOGI(TAG, "publish %s state=%u dist=%umm ltime=%u leader=%s",
             update->spot_id, update->state, update->raw_distance_mm,
             update->header.lamport_ts, leader_mac_str);
    return 0;
}

int wifi_uplink_publish_leader_change(const char *zone_id, const char *new_leader_mac)
{
    /* TODO(embedded team): publish to a dedicated topic / endpoint so the
     * backend can update zones.current_leader_mac in SQLite. */
    ESP_LOGI(TAG, "leader change zone=%s new_leader=%s", zone_id, new_leader_mac);
    return 0;
}
