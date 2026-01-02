
#ifndef __WIFI_HANDLER_H__
#define __WIFI_HANDLER_H__

#include "esp_wifi.h"
#include "esp_wifi_types_generic.h"

typedef void (*wifi_record_handler)(uint16_t scan_number, wifi_ap_record_t *record);

void wifi_handler_init();
void wifi_handler_start_scan(wifi_record_handler wifi_handler);

#endif // __WIFI_HANDLER_H__


