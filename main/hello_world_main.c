
#include <esp_log.h>
#include <esp_netif.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/semphr.h"
#include <lvgl.h>
#include <nvs_flash.h>
#include <string.h>
#include "esp_netif.h"
#include "esp_event.h"

#include "wifi_handler.h"
#include "driver_init.h"
#include "main_ui.h"
#include "heap_monitor.h"

static void app_main_UI_starter(void *params)
{
    main_ui_initialize();

    while (1)
    {
        lv_lock();
        lv_timer_handler();
        lv_unlock();

        vTaskDelay(1);
    }

    vTaskDelete(NULL);
}

void app_main()
{
    ESP_LOGE(__FILE__, "Initializing NVS flash");
    esp_err_t error = nvs_flash_init();
    if ((ESP_ERR_NVS_NO_FREE_PAGES == error) || (ESP_ERR_NVS_NEW_VERSION_FOUND == error))
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        error = nvs_flash_init();
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    heap_monitor_init();  // Captures: "245,678 bytes free"    
    lcd_driver_init();
    
    heap_monitor_print("After LCD");  // Shows: "232,450 bytes free" (-13KB)    
    lvgl_init();

    heap_monitor_print("After LVGL"); // Shows: "198,234 bytes free" (-34KB)    
    touch_spi_init();
    
    
    heap_monitor_print("After Touch SPI driver"); // Shows: "xxx bytes free" (-40KB)
    driver_touch_init();
   
    
    
    heap_monitor_print("After Driver Touch"); // Shows: "xxx bytes free" (-40KB)
    wifi_handler_init();
    
    
    heap_monitor_print("After WiFi"); // Shows: "158,000 bytes free" (-40KB)
    xTaskCreate(app_main_UI_starter, "ui-starter", 4096 * 2, NULL, 3, NULL);
}
