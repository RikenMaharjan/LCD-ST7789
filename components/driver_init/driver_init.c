#include "display/lv_display.h"
#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_dev.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_st7789.h"
#include "esp_lcd_touch.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <lvgl.h>
#include <stdio.h>
#include <string.h>

#include "esp_lcd_touch_xpt2046.h"
#include "indev/lv_indev.h"

#include "pin_setup.h"
#include "driver_touch.h"
#include "driver_init.h"

#define TAG "esp_lcd"

// #define TOUCH_X_RES_MIN         0
// #define TOUCH_X_RES_MAX         240
// #define TOUCH_Y_RES_MIN         0
// #define TOUCH_Y_RES_MAX         320
// #define LCD_H_RES               240 // TOUCH_X_RES_MAX 
// #define LCD_V_RES               320 // TOUCH_Y_RES_MAX
// #define LCD_BITS_PIXEL          16
// #define LCD_BUF_LINES           30
// #define LCD_DOUBLE_BUFFER       1
// #define LCD_DRAWBUF_SIZE        (LCD_H_RES * LCD_BUF_LINES)
// #define LCD_MIRROR_X            (true)
// #define LCD_MIRROR_Y            (false)
// #define LCD_PIXEL_CLOCK_HZ      (40 * 1000 * 1000)
// #define LCD_CMD_BITS            (8)
// #define LCD_PARAM_BITS          (8)
// #define LCD_SPI_HOST                SPI2_HOST
// #define LCD_SPI_CLK             (gpio_num_t) GPIO_NUM_14
// #define LCD_SPI_MOSI            (gpio_num_t) GPIO_NUM_13
// #define LCD_SPI_MISO            (gpio_num_t) GPIO_NUM_12
// #define LCD_DC                  (gpio_num_t) GPIO_NUM_2
// #define LCD_CS                  (gpio_num_t) GPIO_NUM_15
// #define LCD_RESET               (gpio_num_t) GPIO_NUM_4
// #define LCD_BUSY                (gpio_num_t) GPIO_NUM_NC
// #define LCD_BACKLIGHT_PIN       (gpio_num_t) GPIO_NUM_21
// #define LCD_BACKLIGHT_LEDC_CH   (1)
// #define TOUCH_CLOCK_HZ          ESP_LCD_TOUCH_SPI_CLOCK_HZ  // already in default config
// #define TOUCH_SPI_HOST          SPI3_HOST
// #define TOUCH_SPI_CLK          (gpio_num_t) GPIO_NUM_25
// #define TOUCH_SPI_MOSI          (gpio_num_t) GPIO_NUM_32
// #define TOUCH_SPI_MISO          (gpio_num_t) GPIO_NUM_39
// #define TOUCH_CS_PIN            (gpio_num_t) GPIO_NUM_33
// #define TOUCH_INT_PIN           (gpio_num_t) GPIO_NUM_36
// #define TOUCH_DC                (gpio_num_t) GPIO_NUM_NC
// #define TOUCH_RST               (gpio_num_t) GPIO_NUM_NC


// #define LCD_SPI_HOST SPI2_HOST
// #define LCD_SPI_CLK 14 // TFT_sck
// #define LCD_SPI_MOSI 13
// #define LCD_SPI_MISO 12
// #define LCD_DC 2
// #define LCD_CS 15
// #define LCD_BACKLIGHT_PIN 21

// #define TOUCH_SPI_HOST SPI3_HOST
// #define TOUCH_SPI_CLK 25
// #define TOUCH_SPI_MOSI 32
// #define TOUCH_SPI_MISO 39
// #define TOUCH_INT_PIN 36
// #define TOUCH_CS_PIN 33

static lv_display_t *display;
static esp_lcd_panel_io_handle_t lcd_io_handle;
static esp_lcd_panel_io_handle_t touch_io_handle;
static esp_lcd_panel_handle_t lcd_panel_handle;
static esp_lcd_touch_handle_t touch_handle;
// static esp_lcd_touch_handle_t touch_pad;

static void setup_timer();
static void lvgl_touch_cb(lv_indev_t *indev, lv_indev_data_t *data);
static void touch_input_init();
static void lv_tick_task(void *arg);
static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);

void lcd_driver_init(void)
{
    esp_err_t ret = ESP_FAIL;
    ESP_LOGI(TAG, "Initializing LCD");

    spi_bus_config_t lcd_spi_config = {
        .sclk_io_num = LCD_SPI_CLK,
        .mosi_io_num = LCD_SPI_MOSI,
        .miso_io_num = LCD_SPI_MISO,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = LCD_H_RES * 80 * sizeof(uint16_t),
    };
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = LCD_DC,
        .cs_gpio_num = LCD_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_RESET,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = LCD_BITS_PIXEL,
    };
    // Configure backlight GPIO
    gpio_config_t bk_gpio_config = {.mode = GPIO_MODE_OUTPUT, .pin_bit_mask = 1ULL << 21};
    ret = gpio_config(&bk_gpio_config);
    ESP_ERROR_CHECK(ret);
    gpio_set_level(21, 0); // Turn off initially

    // Configure SPI bus
    ret = spi_bus_initialize(LCD_SPI_HOST, &lcd_spi_config, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);

    // Configure LCD IO
    ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &io_config, &lcd_io_handle);
    ESP_ERROR_CHECK(ret);

    // Configure LCD panel
    ret = esp_lcd_new_panel_st7789(lcd_io_handle, &panel_config, &lcd_panel_handle);
    ESP_ERROR_CHECK(ret);

    // Initialize panel
    ret = esp_lcd_panel_reset(lcd_panel_handle);
    ESP_ERROR_CHECK(ret);
    ret = esp_lcd_panel_init(lcd_panel_handle);
    ESP_ERROR_CHECK(ret);

    // Configure display orientation (adjust these if display is rotated/mirrored wrong)
    ret = esp_lcd_panel_swap_xy(lcd_panel_handle, false);
    ESP_ERROR_CHECK(ret);
    ret = esp_lcd_panel_mirror(lcd_panel_handle, false, false);
    ESP_ERROR_CHECK(ret);
    ret = esp_lcd_panel_invert_color(lcd_panel_handle, false);
    ESP_ERROR_CHECK(ret);
    //
    // Gap settings for ST7789 (may need adjustment based on your specific display)
    ret = esp_lcd_panel_set_gap(lcd_panel_handle, 0, 0);
    ESP_ERROR_CHECK(ret);

    // Turn on display
    ret = esp_lcd_panel_disp_on_off(lcd_panel_handle, true);
    ESP_ERROR_CHECK(ret);

    // Turn on backlight
    gpio_set_level(LCD_BACKLIGHT, 1);

    ESP_LOGI(TAG, "LCD initialization complete");
}

void lvgl_init(void)
{
    lv_init();
    setup_timer();

    display = lv_display_create(LCD_H_RES, LCD_V_RES);
#warning "why 20 not 80"?;
    size_t draw_buffer_sz = LCD_V_RES * 20 * sizeof(lv_color16_t); 
    void *buf1 = spi_bus_dma_memory_alloc(LCD_SPI_HOST, draw_buffer_sz, 0);
    assert(buf1);
    void *buf2 = spi_bus_dma_memory_alloc(LCD_SPI_HOST, draw_buffer_sz, 0);
    assert(buf2);

    // initialize LVGL draw buffers
    lv_display_set_buffers(display, buf1, buf2, draw_buffer_sz, LV_DISPLAY_RENDER_MODE_PARTIAL);

    // Connect ESP panel handle to LVGL display
    lv_display_set_user_data(display, lcd_panel_handle);
    lv_display_set_flush_cb(display, flush_cb);

    // Clear the screen first
    lv_obj_clean(lv_screen_active());
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), 0);

    // // Force initial render
    lv_refr_now(display);
}

void lvgl_task(void *pvParameter)
{
    ESP_LOGI(TAG, "LVGL task started");

    while (1)
    {
        uint32_t time_till_next = lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(time_till_next > 0 ? time_till_next : 5));
    }
}

void driver_touch_init(void)
{
    // esp_lcd_touch_config_t tp_cfg = {
    //     .x_max = LCD_H_RES,
    //     .y_max = LCD_V_RES,
    //     .rst_gpio_num = -1,
    //     .int_gpio_num = TOUCH_INT_PIN,
    //     .flags =
    //         {
    //             .swap_xy = 0,
    //             .mirror_x = 0,
    //             .mirror_y = 0,
    //         },
    // };
    // esp_lcd_panel_io_spi_config_t tp_io_config = ESP_LCD_TOUCH_IO_SPI_XPT2046_CONFIG(TOUCH_CS_PIN);
    // esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)TOUCH_SPI_HOST, &tp_io_config, &touch_io_handle);
    // ESP_ERROR_CHECK(esp_lcd_touch_new_spi_xpt2046(touch_io_handle, &tp_cfg, &touch_handle));
    // touch_pad = touch_handle; // Properly assign touch_handle to touch_pad
    // touch_input_init();

    lcd_touch_init(&touch_handle);
    ESP_LOGI(TAG, "Initialize touch controller XPT2046");
}

static void setup_timer()
{
    const esp_timer_create_args_t periodic_timer_args = {.callback = &lv_tick_task, .name = "lv_tick"};
    esp_timer_handle_t periodic_timer;
    esp_timer_create(&periodic_timer_args, &periodic_timer);
    esp_timer_start_periodic(periodic_timer, 1000); // 1000 us = 1 ms
}

static void lvgl_touch_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    uint16_t touchpad_x[1] = {0};
    uint16_t touchpad_y[1] = {0};
    uint8_t touchpad_cnt = 0;

    esp_lcd_touch_read_data(touch_handle);
    /* Get coordinates */
    bool touchpad_pressed = esp_lcd_touch_get_coordinates(touch_handle, touchpad_x, touchpad_y, NULL, &touchpad_cnt, 1);

    if (touchpad_pressed && touchpad_cnt > 0)
    {
        data->point.x = LCD_H_RES - 1 - touchpad_x[0];
        data->point.y = touchpad_y[0];
        data->state = LV_INDEV_STATE_PRESSED;
        // esp_rom_printf("%d, %d\n", data->point.x, data->point.y);
    }
    else
    {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void touch_input_init()
{
    static lv_indev_t *indev;
    indev = lv_indev_create(); // Input device driver (Touch)
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    assert(display);
    lv_indev_set_display(indev, display);
    lv_indev_set_user_data(indev, touch_handle);
    lv_indev_set_read_cb(indev, lvgl_touch_cb);
}

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    lcd_panel_handle = lv_display_get_user_data(disp);

    int w = area->x2 - area->x1 + 1;
    int h = area->y2 - area->y1 + 1;

    // Swap RGB565 byte order for ST7789
    lv_draw_sw_rgb565_swap(px_map, w * h);

    // Draw to LCD
    esp_lcd_panel_draw_bitmap(lcd_panel_handle, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map);

    lv_display_flush_ready(disp);
}

static void lv_tick_task(void *arg)
{
    lv_tick_inc(1); // increment LVGL tick by 1 ms
}
