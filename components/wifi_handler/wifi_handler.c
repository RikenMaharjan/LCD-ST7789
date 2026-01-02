
#include "esp_wifi.h"
#include "esp_log.h"

#include "wifi_handler.h"

#define TAG "WiFi-Handler"

static void wifi_event_handler(void *args, esp_event_base_t event_base, int32_t event_id, void *event_data);

const wifi_scan_config_t scan_config = {
    .show_hidden = false,
    .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    .scan_time.active.max = 120,
    .scan_time.active.min = 0,
};

void wifi_handler_init()
{
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "WiFiStaSSID",
            .password = "WiFiStaPass",
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static wifi_record_handler record_handler = NULL;

void wifi_handler_start_scan(wifi_record_handler wifi_handler)
{
    assert(wifi_handler);
    record_handler = wifi_handler;
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, false));
}

static void wifi_event_handler(void *args, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (WIFI_EVENT == event_base)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START:
        {
            ESP_LOGE(TAG, "WiFi started...");
            break;
        }
        case WIFI_EVENT_SCAN_DONE:
        {
            ESP_LOGE(TAG, "WiFi scan done event");

            uint16_t scan_number = 0;
            ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&scan_number));
            wifi_ap_record_t *record = (wifi_ap_record_t*)malloc(scan_number * sizeof(wifi_ap_record_t));
            assert(record);
            ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&scan_number, record));
            record_handler(scan_number, record);
            free(record);
            esp_wifi_scan_stop();
            break;
        }
        default:
        {
            ESP_LOGE(TAG, "Unable to handle wifi event id %ld", event_id);
            break;
        }
        }
    }
    else if (IP_EVENT == event_base)
    {
        ESP_LOGE(TAG, "Incoming IP_EVENT id: %ld", event_id);
    }
}
