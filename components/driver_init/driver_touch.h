#ifndef _DRIVER_TOUCH_H_    
#define _DRIVER_TOUCH_H_

#include <esp_err.h>
#include <esp_lcd_touch.h>

esp_err_t custom_touch_init(esp_lcd_touch_handle_t *tp);

#endif /*_DRIVER_TOUCH_H_*/