/*
 * leader-node main.c — boot sequence and main loop.
 *
 * Wires together: WiFi init, ESP-NOW init, radio scheduler, heartbeat,
 * bully, aggregator, uplink. The interesting logic lives in those modules;
 * this file is just the conductor.
 *
 * All tunables come from config.h.
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "config.h"
#include "messages.h"
#include "lamport.h"
#include "radio_scheduler.h"
#include "espnow_handler.h"
#include "wifi_uplink.h"
#include "bully.h"
#include "heartbeat.h"
#include "aggregator.h"

static const char *TAG = "leader";

static uint32_t priority_from_mac(const uint8_t mac[6])
{
    /* Low 4 bytes of MAC. Deterministic and unique within a zone. */
    return ((uint32_t)mac[2] << 24) | ((uint32_t)mac[3] << 16)
         | ((uint32_t)mac[4] << 8)  |  (uint32_t)mac[5];
}

static void wait_for_wifi_ready(uint32_t timeout_ms)
{
    uint32_t waited_ms = 0;
    while (!wifi_uplink_is_ready() && waited_ms < timeout_ms) {
        vTaskDelay(pdMS_TO_TICKS(250));
        waited_ms += 250;
    }
    ESP_LOGI(TAG, "wifi preflight %s after %u ms",
             wifi_uplink_is_ready() ? "ready" : "not ready", (unsigned)waited_ms);
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

    /* WiFi must be initialized in STA mode before either ESP-NOW or the
     * uplink can use the radio. The radio scheduler then time-shares
     * between ESP-NOW and an associated WiFi connection. */
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wcfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    esp_wifi_set_country_code("US", true);
#if CONFIG_SOC_WIFI_SUPPORT_5G
    esp_err_t band_err = esp_wifi_set_band_mode(WIFI_BAND_MODE_2G_ONLY);
    if (band_err == ESP_OK) {
        ESP_LOGI(TAG, "wifi band mode: 2.4GHz only");
    } else {
        ESP_LOGW(TAG, "wifi 2.4GHz band mode failed: %s", esp_err_to_name(band_err));
    }
#endif
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));

    uint8_t my_mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, my_mac);
    uint32_t my_priority = priority_from_mac(my_mac);
    ESP_LOGI(TAG, "leader boot: zone=%s/%s priority=0x%08x mac=%02x:%02x:%02x:%02x:%02x:%02x",
             ZONE_LOT, ZONE_ID, (unsigned)my_priority,
             my_mac[0], my_mac[1], my_mac[2], my_mac[3], my_mac[4], my_mac[5]);

    aggregator_init();
    wifi_uplink_init(WIFI_SSID, WIFI_PASSWORD);
    wait_for_wifi_ready(20000);

    espnow_handler_init();
    bully_init(my_priority);
    heartbeat_init(my_priority);
    radio_scheduler_start();

    /* Main loop: poll heartbeat timeout and drain aggregator. */
    while (1) {
        heartbeat_check_timeout();
        aggregator_tick();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
