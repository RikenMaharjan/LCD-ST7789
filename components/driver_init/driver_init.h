#ifndef __DRIVER_INIT_H__
#define __DRIVER_INIT_H__

#define LCD_H_RES 240
#define LCD_V_RES 320

void lvgl_init(void);
void lvgl_task(void *pvParameter);
esp_err_t lcd_driver_init(void);
void touch_spi_init(void);
void driver_touch_init();

#endif
