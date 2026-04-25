/*
 * leader-node main.c — boot sequence and main loop.
 *
 * Wires together: WiFi init, ESP-NOW init, radio scheduler, heartbeat,
 * bully, aggregator, uplink. The interesting logic lives in those modules;
 * this file is just the conductor.
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "messages.h"
#include "lamport.h"
#include "radio_scheduler.h"
#include "espnow_handler.h"
#include "wifi_uplink.h"
#include "bully.h"
#include "heartbeat.h"
#include "aggregator.h"

static const char *TAG = "leader";

/* Zone identity — one of these per flashed leader. TODO(team): move to NVS
 * once we have more than one zone hardware-provisioned. */
#define ZONE_LOT   "lotA"
#define ZONE_ID    "zone1"

/* TODO(team): these must come from secrets management in production.
 * For class bring-up, set them here and don't commit real credentials. */
#define WIFI_SSID     "parking-lot-net"
#define WIFI_PASSWORD "change-me"
#define BACKEND_HOST  "192.168.1.10"
#define BACKEND_PORT  1883   /* MQTT default; 8000 if UPLINK_PROTOCOL_REST */

static uint32_t priority_from_mac(const uint8_t mac[6])
{
    /* Use the low 4 bytes of the MAC as node priority. Deterministic and
     * unique within a zone. */
    return ((uint32_t)mac[2] << 24) | ((uint32_t)mac[3] << 16)
         | ((uint32_t)mac[4] << 8)  |  (uint32_t)mac[5];
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    lamport_init();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wcfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    uint8_t my_mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, my_mac);
    uint32_t my_priority = priority_from_mac(my_mac);
    ESP_LOGI(TAG, "leader boot: zone=%s/%s priority=0x%08x",
             ZONE_LOT, ZONE_ID, (unsigned)my_priority);

    espnow_handler_init();
    aggregator_init();
    bully_init(my_priority);
    heartbeat_init(my_priority);
    wifi_uplink_init(WIFI_SSID, WIFI_PASSWORD, BACKEND_HOST, BACKEND_PORT);
    radio_scheduler_start();

    /* Main loop: poll heartbeat timeout and drain aggregator. */
    while (1) {
        heartbeat_check_timeout();
        aggregator_tick();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
