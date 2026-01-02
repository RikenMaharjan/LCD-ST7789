# LCD-ST7789 Project - Complete Workflow Report

## Table of Contents
1. [Project Overview](#project-overview)
2. [System Architecture](#system-architecture)
3. [Initialization Flow](#initialization-flow)
4. [Component Details](#component-details)
5. [UI System Architecture](#ui-system-architecture)
6. [Data Flow](#data-flow)
7. [Hardware Configuration](#hardware-configuration)

---

## Project Overview

### Basic Information
- **Platform**: ESP32 (ESP-IDF v5.4.x)
- **Display**: ST7789 LCD - 240x320 pixels
- **Touch Controller**: XPT2046
- **Graphics Library**: LVGL (Light and Versatile Graphics Library)
- **Build System**: CMake + ESP-IDF

### Project Purpose
This is an embedded touchscreen UI application for ESP32 featuring:
- LCD display management with ST7789 driver
- Touch input handling via XPT2046
- WiFi connectivity management
- Tile-based user interface using LVGL
- FreeRTOS-based multitasking

---

## System Architecture

### High-Level System Diagram

```
┌────────────────────────────────────────────────────────────┐
│                      ESP32 MCU                              │
│  ┌──────────────────────────────────────────────────────┐  │
│  │              Application Layer                        │  │
│  │           (hello_world_main.c)                        │  │
│  └───────────────┬───────────────────┬──────────────────┘  │
│                  │                   │                      │
│    ┌─────────────▼────────┐   ┌─────▼──────────────┐      │
│    │   Main UI Component  │   │  WiFi Handler       │      │
│    │   ┌──────────────┐   │   │  Component          │      │
│    │   │ Main Screen  │   │   │  - Init WiFi        │      │
│    │   │ Utility Bar  │   │   │  - Event Handler    │      │
│    │   │ WiFi Tile    │   │   │  - Scan Function    │      │
│    │   │ BLE Tile     │   │   └─────────────────────┘      │
│    │   │ Other Tile   │   │                                │
│    │   └──────────────┘   │                                │
│    └───────────┬──────────┘                                │
│                │                                            │
│    ┌───────────▼──────────────────────────────┐            │
│    │       Driver Init Component              │            │
│    │   ┌──────────────────────────────────┐   │            │
│    │   │  LCD Driver (ST7789)             │   │            │
│    │   │  - SPI2 Interface                │   │            │
│    │   │  - 240x320 Resolution            │   │            │
│    │   └──────────────────────────────────┘   │            │
│    │   ┌──────────────────────────────────┐   │            │
│    │   │  LVGL Graphics Engine            │   │            │
│    │   │  - Display Management            │   │            │
│    │   │  - Rendering Pipeline            │   │            │
│    │   └──────────────────────────────────┘   │            │
│    │   ┌──────────────────────────────────┐   │            │
│    │   │  Touch Driver (XPT2046)          │   │            │
│    │   │  - SPI3 Interface                │   │            │
│    │   │  - Input Event Generation        │   │            │
│    │   └──────────────────────────────────┘   │            │
│    └──────────────────────────────────────────┘            │
└────────────────────────────────────────────────────────────┘
         │              │              │
    ┌────▼────┐    ┌───▼────┐    ┌───▼─────┐
    │ ST7789  │    │XPT2046 │    │  WiFi   │
    │   LCD   │    │ Touch  │    │  Radio  │
    └─────────┘    └────────┘    └─────────┘
```

---

## Initialization Flow

### System Boot Sequence

```
                    START
                      │
                      ▼
          ┌───────────────────────┐
          │    app_main()         │
          │   Entry Point         │
          └───────────┬───────────┘
                      │
        ┌─────────────┼─────────────┐
        │             │             │
        ▼             ▼             ▼
┌──────────────┐ ┌────────────┐ ┌──────────────┐
│ NVS Flash    │ │  Network   │ │   Hardware   │
│ Initialization│ │   Stack    │ │  Drivers     │
└──────┬───────┘ └─────┬──────┘ └──────┬───────┘
       │               │               │
       │         ┌─────▼─────┐         │
       │         │esp_netif_ │         │
       │         │   init()  │         │
       │         └─────┬─────┘         │
       │               │               │
       │         ┌─────▼────────┐      │
       │         │esp_event_loop│      │
       │         │   _create()  │      │
       │         └──────────────┘      │
       │                               │
       │         ┌─────────────────────┤
       │         │                     │
       │    ┌────▼─────┐       ┌──────▼──────┐
       │    │LCD Driver│       │LVGL Init    │
       │    │  Init    │       │             │
       │    └────┬─────┘       └──────┬──────┘
       │         │                    │
       │    ┌────▼─────┐       ┌──────▼──────┐
       │    │  Touch   │       │Touch Input  │
       │    │  Driver  │       │Device Init  │
       │    └──────────┘       └─────────────┘
       │
   ┌───▼────────────┐
   │WiFi Handler    │
   │Initialization  │
   └───┬────────────┘
       │
       ▼
┌──────────────────┐
│  xTaskCreate()   │
│  UI Starter Task │
│  Priority: 3     │
│  Stack: 8KB      │
└───┬──────────────┘
    │
    ▼
┌──────────────────┐
│main_ui_          │
│initialize()      │
└───┬──────────────┘
    │
    ▼
┌──────────────────┐
│ Infinite Loop:   │
│ ┌──────────────┐ │
│ │lv_lock()     │ │
│ │lv_timer_     │ │
│ │  handler()   │ │
│ │lv_unlock()   │ │
│ │vTaskDelay(1) │ │
│ └──────────────┘ │
└──────────────────┘
```

### Detailed Initialization Steps

```
Step 1: NVS Flash (Non-Volatile Storage)
┌─────────────────────────────────────┐
│ nvs_flash_init()                    │
│ - Initialize flash storage          │
│ - Handle corruption/version issues  │
│ - Erase if needed                   │
└─────────────────────────────────────┘

Step 2: Network Stack Setup
┌─────────────────────────────────────┐
│ esp_netif_init()                    │
│ - Initialize TCP/IP adapter         │
│                                     │
│ esp_event_loop_create_default()     │
│ - Create default event loop         │
│ - Enable event-driven architecture  │
└─────────────────────────────────────┘

Step 3: LCD Driver Initialization
┌─────────────────────────────────────┐
│ lcd_driver_init()                   │
│ 1. Configure SPI2 bus               │
│    - SCLK: GPIO 14                  │
│    - MOSI: GPIO 13                  │
│    - MISO: GPIO 12                  │
│    - Clock: 40 MHz                  │
│ 2. Configure Panel IO               │
│    - DC pin: GPIO 2                 │
│    - CS pin: GPIO 15                │
│ 3. Initialize ST7789 panel          │
│    - Reset sequence                 │
│    - Configuration commands         │
│ 4. Configure backlight (GPIO 21)    │
│ 5. Turn on display                  │
└─────────────────────────────────────┘

Step 4: LVGL Graphics Library
┌─────────────────────────────────────┐
│ lvgl_init()                         │
│ 1. Call lv_init()                   │
│ 2. Create 1ms timer for tick        │
│ 3. Create display object (240x320)  │
│ 4. Allocate DMA draw buffers:       │
│    - Buffer 1: 320*20*2 bytes       │
│    - Buffer 2: 320*20*2 bytes       │
│ 5. Register flush callback          │
│ 6. Clear screen (black background)  │
└─────────────────────────────────────┘

Step 5: Touch Controller
┌─────────────────────────────────────┐
│ touch_spi_init() +                  │
│ driver_touch_init()                 │
│ 1. Configure SPI3 bus               │
│    - SCLK: GPIO 25                  │
│    - MOSI: GPIO 32                  │
│    - MISO: GPIO 39                  │
│    - CS:   GPIO 33                  │
│    - INT:  GPIO 36                  │
│ 2. Initialize XPT2046 controller    │
│ 3. Configure resolution mapping     │
│ 4. Create LVGL input device         │
│    - Type: Pointer                  │
│    - Callback: lvgl_touch_cb()      │
└─────────────────────────────────────┘

Step 6: WiFi Subsystem
┌─────────────────────────────────────┐
│ wifi_handler_init()                 │
│ 1. Create STA network interface     │
│ 2. Initialize WiFi with default cfg │
│ 3. Register event handlers:         │
│    - WIFI_EVENT (all IDs)           │
│    - IP_EVENT (all IDs)             │
│ 4. Set mode to WIFI_MODE_STA        │
│ 5. Configure credentials            │
│ 6. Start WiFi subsystem             │
└─────────────────────────────────────┘

Step 7: UI Task Creation
┌─────────────────────────────────────┐
│ xTaskCreate()                       │
│ - Task: app_main_UI_starter         │
│ - Stack Size: 8192 bytes            │
│ - Priority: 3                       │
│ - Core: Auto-assigned               │
└─────────────────────────────────────┘

Step 8: Main UI Initialization
┌─────────────────────────────────────┐
│ main_ui_initialize()                │
│ 1. Get active screen                │
│ 2. Set flex column layout           │
│ 3. Create utility bar               │
│ 4. Create main menu area (grid)     │
│ 5. Create tiles:                    │
│    - WiFi Tile                      │
│    - Bluetooth Tile                 │
│    - Other Tile                     │
│ 6. Start message deletion timer     │
└─────────────────────────────────────┘
```

---

## Component Details

### 1. Main Application (`main/hello_world_main.c`)

```
┌──────────────────────────────────────┐
│         app_main()                   │
│  Entry point of the application      │
│                                      │
│  Responsibilities:                   │
│  ✓ System initialization            │
│  ✓ Component setup orchestration    │
│  ✓ Task creation                    │
└──────────────────────────────────────┘

┌──────────────────────────────────────┐
│    app_main_UI_starter Task          │
│  FreeRTOS Task - Priority 3          │
│                                      │
│  while(1) {                          │
│    lv_lock();        ← Mutex lock    │
│    lv_timer_handler();← Process UI   │
│    lv_unlock();      ← Release mutex │
│    vTaskDelay(1);    ← 1ms delay     │
│  }                                   │
│                                      │
│  Purpose:                            │
│  - Continuously processes LVGL timers│
│  - Handles UI updates and rendering  │
│  - Ensures thread-safe LVGL access   │
└──────────────────────────────────────┘
```

### 2. Driver Init Component (`components/driver_init/`)

**File Structure:**
```
driver_init/
├── driver_init.c   (Implementation)
├── driver_init.h   (Header file)
└── CMakeLists.txt  (Build configuration)
```

**Pin Configuration:**

| Component | Function | GPIO | Notes |
|-----------|----------|------|-------|
| LCD       | SCLK     | 14   | SPI Clock |
| LCD       | MOSI     | 13   | Data to LCD |
| LCD       | MISO     | 12   | Data from LCD (unused for ST7789) |
| LCD       | DC       | 2    | Data/Command select |
| LCD       | CS       | 15   | Chip select |
| LCD       | Backlight| 21   | PWM capable (used as ON/OFF) |
| Touch     | SCLK     | 25   | SPI Clock |
| Touch     | MOSI     | 32   | Data to touch controller |
| Touch     | MISO     | 39   | Data from touch controller |
| Touch     | INT      | 36   | Interrupt (touch detected) |
| Touch     | CS       | 33   | Chip select |

**LCD Driver Flow:**

```
lcd_driver_init()
│
├─ Configure SPI Bus
│  └─ spi_bus_initialize(SPI2_HOST)
│     ├─ SCLK: 14, MOSI: 13, MISO: 12
│     ├─ Max transfer: 240*80*2 bytes
│     └─ DMA: AUTO
│
├─ Configure Panel IO
│  └─ esp_lcd_new_panel_io_spi()
│     ├─ DC: GPIO 2
│     ├─ CS: GPIO 15
│     ├─ Clock: 40 MHz
│     └─ Queue depth: 10
│
├─ Create ST7789 Panel
│  └─ esp_lcd_new_panel_st7789()
│     ├─ RGB order: RGB
│     └─ Bits per pixel: 16 (RGB565)
│
├─ Initialize Panel
│  ├─ esp_lcd_panel_reset()
│  ├─ esp_lcd_panel_init()
│  ├─ esp_lcd_panel_swap_xy(false)
│  ├─ esp_lcd_panel_mirror(false, false)
│  ├─ esp_lcd_panel_invert_color(false)
│  └─ esp_lcd_panel_set_gap(0, 0)
│
└─ Enable Display
   ├─ esp_lcd_panel_disp_on_off(true)
   └─ gpio_set_level(21, 1) ← Turn on backlight
```

**LVGL Initialization:**

```
lvgl_init()
│
├─ Initialize LVGL Library
│  └─ lv_init()
│
├─ Setup Timer (1ms tick)
│  └─ esp_timer_create()
│     └─ Callback: lv_tick_task()
│        └─ Calls: lv_tick_inc(1)
│
├─ Create Display
│  └─ lv_display_create(240, 320)
│     └─ Color format: RGB565
│
├─ Allocate Draw Buffers
│  ├─ Buffer 1: spi_bus_dma_memory_alloc()
│  │  └─ Size: 320 * 20 * 2 = 12,800 bytes
│  ├─ Buffer 2: spi_bus_dma_memory_alloc()
│  │  └─ Size: 320 * 20 * 2 = 12,800 bytes
│  └─ lv_display_set_buffers()
│     └─ Mode: PARTIAL rendering
│
├─ Register Flush Callback
│  └─ lv_display_set_flush_cb(flush_cb)
│
└─ Initialize Screen
   ├─ lv_obj_clean(lv_screen_active())
   ├─ Set background: BLACK
   └─ lv_refr_now() ← Force render
```

**Flush Callback (LCD Update):**

```
flush_cb(disp, area, px_map)
│
├─ Get panel handle
│  └─ lcd_panel_handle = lv_display_get_user_data(disp)
│
├─ Calculate dimensions
│  ├─ w = area->x2 - area->x1 + 1
│  └─ h = area->y2 - area->y1 + 1
│
├─ Swap RGB565 byte order
│  └─ lv_draw_sw_rgb565_swap(px_map, w*h)
│     (ST7789 expects different byte order)
│
├─ Draw to LCD panel
│  └─ esp_lcd_panel_draw_bitmap()
│     ├─ Start: (x1, y1)
│     ├─ End: (x2+1, y2+1)
│     └─ Data: px_map
│
└─ Signal completion
   └─ lv_display_flush_ready(disp)
```

### 3. WiFi Handler Component (`components/wifi_handler/`)

```
wifi_handler_init()
│
├─ Create Station Interface
│  └─ esp_netif_create_default_wifi_sta()
│
├─ Initialize WiFi
│  └─ esp_wifi_init(WIFI_INIT_CONFIG_DEFAULT)
│
├─ Register Event Handlers
│  ├─ esp_event_handler_register(WIFI_EVENT, ...)
│  └─ esp_event_handler_register(IP_EVENT, ...)
│
├─ Configure WiFi
│  └─ wifi_config_t:
│     ├─ SSID: "WiFiStaSSID"
│     ├─ Password: "WiFiStaPass"
│     └─ Auth: WPA2_PSK
│
├─ Set Mode
│  └─ esp_wifi_set_mode(WIFI_MODE_STA)
│
├─ Apply Config
│  └─ esp_wifi_set_config(WIFI_IF_STA, &wifi_config)
│
└─ Start WiFi
   └─ esp_wifi_start()
```

**WiFi Event Handling:**

```
                WiFi Events
                     │
        ┌────────────┼────────────┐
        │            │            │
        ▼            ▼            ▼
┌───────────┐ ┌──────────┐ ┌──────────┐
│ STA_START │ │SCAN_DONE │ │ IP_EVENT │
└─────┬─────┘ └────┬─────┘ └────┬─────┘
      │            │            │
      │            ▼            │
      │     ┌──────────────┐   │
      │     │Get AP number │   │
      │     └──────┬───────┘   │
      │            │            │
      │     ┌──────▼───────┐   │
      │     │Get AP records│   │
      │     └──────┬───────┘   │
      │            │            │
      │     ┌──────▼───────┐   │
      │     │Call callback │   │
      │     │with records  │   │
      │     └──────────────┘   │
      │                        │
      ▼                        ▼
   (Logged)                (Logged)
```

### 4. Main UI Component (`components/main-ui/`)

**File Structure:**
```
main-ui/
├── main_ui.c                 # Main UI initialization
├── main_ui.h                 # Header and macros
├── main_event_handlers.c     # Event callbacks
├── wifi_ui.c                 # WiFi configuration UI
├── UI_commons.c              # Common UI utilities
├── tile_uis/
│   ├── wifi_tile.c           # WiFi tile implementation
│   ├── ble_tile.c            # Bluetooth tile implementation
│   ├── other_tile.c          # Other settings tile
│   ├── wifi_icons.c          # WiFi icon assets
│   └── bluetooth_icons.c     # Bluetooth icon assets
└── CMakeLists.txt
```

**UI Helper Macros:**
```c
// Object definition macro
DEFINE_OBJECT(wifi_tile);
// Expands to: static lv_obj_t *object_wifi_tile = NULL;

// Style definition macro
DEFINE_STYLE(wifi_tile);
// Expands to: static lv_style_t *style_wifi_tile = NULL;

// Style allocation macro
MALLOC_STYLE(style_wifi_tile);
// Expands to: style_wifi_tile = lv_malloc(sizeof(lv_style_t));

// Color helper macros
COLOR_SLATE_GRAY()  // RGB(112, 128, 144)
COLOR_LIGHT_GRAY()  // RGB(211, 211, 211)
```

---

## UI System Architecture

### Screen Layout

```
┌──────────────────────────────────────────────┐
│  Main Screen (240x320)                       │
│  Layout: FLEX COLUMN                         │
│                                              │
│  ┌────────────────────────────────────────┐  │
│  │  Utility Bar (240x34)                  │  │
│  │  Layout: FLEX ROW                      │  │
│  │  ┌────┐ ┌────┐ ┌──────────────────┐   │  │
│  │  │WiFi│ │ BLE│ │ Scrolling Message│   │  │
│  │  │Icon│ │Icon│ │     Label        │   │  │
│  │  │ 📶 │ │ 🔵 │ │                  │   │  │
│  │  └────┘ └────┘ └──────────────────┘   │  │
│  └────────────────────────────────────────┘  │
│             5px padding                      │
│  ┌────────────────────────────────────────┐  │
│  │  Main Menu Display (240x281)           │  │
│  │  Layout: GRID (4 columns x 6 rows)     │  │
│  │                                        │  │
│  │  ┌──────────────────────────────────┐ │  │
│  │  │  WiFi Tile (Rows 0-1, Cols 0-3)  │ │  │
│  │  │  ┌────┐                          │ │  │
│  │  │  │WiFi│ WiFi        [Switch ON] │ │  │
│  │  │  │ 📶 │                          │ │  │
│  │  │  └────┘ NABIN_NTFiberNet         │ │  │
│  │  │      [Open Setting Button]       │ │  │
│  │  └──────────────────────────────────┘ │  │
│  │                                        │  │
│  │  ┌──────────────────────────────────┐ │  │
│  │  │  BLE Tile (Rows 2-3, Cols 0-3)   │ │  │
│  │  │  ┌────┐                          │ │  │
│  │  │  │BLE │ BLE         [Switch OFF]│ │  │
│  │  │  │ 🔵 │                          │ │  │
│  │  │  └────┘ NABINs_BLEDevice         │ │  │
│  │  │      [Open Setting Button]       │ │  │
│  │  └──────────────────────────────────┘ │  │
│  │                                        │  │
│  │  ┌──────────────────────────────────┐ │  │
│  │  │  Other Tile (Rows 4-5, Cols 0-3) │ │  │
│  │  │                                  │ │  │
│  │  │    (Reserved for future use)     │ │  │
│  │  │                                  │ │  │
│  │  └──────────────────────────────────┘ │  │
│  └────────────────────────────────────────┘  │
└──────────────────────────────────────────────┘
```

### WiFi Tile Internal Layout

```
┌──────────────────────────────────────┐
│       WiFi Tile (Grid 4x4)           │
│                                      │
│  Column:  0      1      2      3     │
│  Row 0   ┌────┐ WiFi          ┌───┐ │
│          │    │ Label         │Swi│ │
│  Row 1   │WiFi│               │tch│ │
│          │Icon│               └───┘ │
│          └────┘                      │
│  ─────────────────────────────────   │
│  Row 2         SSID Display          │
│           "NABIN_NTFiberNet"         │
│           (Scrolling label)          │
│  ─────────────────────────────────   │
│  Row 3   ┌──────────────────────┐   │
│          │  [Open Setting]      │   │
│  Row 4   │      Button          │   │
│          └──────────────────────┘   │
└──────────────────────────────────────┘

Grid Configuration:
- Columns: [FR(1), FR(1), FR(1), FR(1)]
  (FR = Fractional, equal distribution)
- Rows: [FR(1), FR(1), FR(1), FR(1)]
```

### Main UI Initialization Flow

```
main_ui_initialize()
│
├─ Get Active Screen
│  └─ object_main_screen = lv_scr_act()
│
├─ Configure Layout
│  ├─ lv_obj_set_layout(FLEX)
│  ├─ lv_obj_set_flex_flow(COLUMN)
│  └─ lv_obj_set_style_pad_row(5px)
│
├─ Create Utility Bar
│  └─ main_ui_initialize_utility_bar()
│     ├─ Create container (240x34)
│     ├─ Set layout: FLEX ROW
│     ├─ Create WiFi icon button
│     ├─ Create Bluetooth icon button
│     └─ Create scrolling label (165x24)
│
├─ Create Main Menu Area
│  └─ main_ui_main_menu_area()
│     ├─ Create container (240x281)
│     ├─ Set layout: GRID
│     ├─ Columns: 4 (equal fractions)
│     └─ Rows: 6 (50px each)
│
├─ Apply Main Screen Style
│  └─ Background: LIGHT_GRAY
│
├─ Create Tiles
│  ├─ tile_ui_create_wifi_tile()
│  │  ├─ Position: Col 0-3, Row 0-1
│  │  ├─ Create icon (image)
│  │  ├─ Create label ("WiFi")
│  │  ├─ Create enable switch
│  │  ├─ Create SSID label
│  │  └─ Create open button
│  │
│  ├─ tile_ui_create_bluetooth_tile()
│  │  ├─ Position: Col 0-3, Row 2-3
│  │  ├─ Create icon (image)
│  │  ├─ Create label ("BLE")
│  │  ├─ Create enable switch
│  │  ├─ Create device label
│  │  └─ Create open button
│  │
│  └─ tile_ui_create_other_tile()
│     └─ Position: Col 0-3, Row 4-5
│
└─ Create Message Timer
   └─ lv_timer_create(main_ui_message_deleter, 5000ms)
```

---

## Data Flow

### Touch Input to Display Update Flow

```
┌─────────────────────┐
│   User Touch        │
│   on Screen         │
└──────────┬──────────┘
           │
           ▼
┌───────────────────────┐
│  XPT2046 Controller   │
│  - Detects touch      │
│  - Triggers INT       │
└──────────┬────────────┘
           │ SPI3 Read
           ▼
┌───────────────────────┐
│ esp_lcd_touch_read_   │
│ data()                │
│ - Read raw X/Y        │
└──────────┬────────────┘
           │
           ▼
┌───────────────────────┐
│ esp_lcd_touch_get_    │
│ coordinates()         │
│ - Convert to pixels   │
└──────────┬────────────┘
           │
           ▼
┌───────────────────────┐
│ lvgl_touch_cb()       │
│ - Transform coords:   │
│   x = 239 - raw_x     │
│   y = raw_y           │
│ - Set press state     │
└──────────┬────────────┘
           │
           ▼
┌───────────────────────┐
│ LVGL Input Processing │
│ - Detect object       │
│ - Generate events     │
└──────────┬────────────┘
           │
     ┌─────┴─────┐
     │           │
     ▼           ▼
┌─────────┐ ┌─────────┐
│ Button  │ │ Switch  │
│ Events  │ │ Events  │
└────┬────┘ └────┬────┘
     │           │
     ▼           ▼
┌────────────────────────┐
│   Event Handlers       │
│ - event_handler_       │
│   switch_events()      │
└────────┬───────────────┘
         │
         ▼
┌────────────────────────┐
│   Callback Functions   │
│ - wifi_enable_button_  │
│   clicked()            │
│ - Update UI elements   │
│ - Change icons         │
│ - Set messages         │
└────────┬───────────────┘
         │
         ▼
┌────────────────────────┐
│  LVGL Rendering        │
│ - lv_timer_handler()   │
│ - Check dirty areas    │
│ - Render to buffer     │
└────────┬───────────────┘
         │
         ▼
┌────────────────────────┐
│     flush_cb()         │
│ - Swap byte order      │
│ - Write to LCD panel   │
└────────┬───────────────┘
         │ SPI2 Write
         ▼
┌────────────────────────┐
│    ST7789 LCD          │
│    Display Updated     │
└────────────────────────┘
```

### WiFi Scan Data Flow (Conceptual)

```
┌────────────────────┐
│  User Toggles      │
│  WiFi Switch       │
└─────────┬──────────┘
          │
          ▼
┌──────────────────────────┐
│ event_handler_switch_    │
│ events()                 │
│ - Read switch state      │
│ - Store in user data     │
└─────────┬────────────────┘
          │
          ▼
┌──────────────────────────┐
│ wifi_enable_button_      │
│ clicked()                │
│ - Check state            │
│ - Update UI (icon, msg)  │
└─────────┬────────────────┘
          │
          ├─ IF ON:
          │  └─> Change icon to "connected"
          │  └─> Enable "Open Setting" button
          │  └─> Message: "WiFi turned on"
          │
          └─ IF OFF:
             └─> Change icon to "disconnected"
             └─> Disable "Open Setting" button
             └─> Message: "WiFi turned off"
```

### Message Display System

```
┌──────────────────────────┐
│  Component calls         │
│  main_ui_set_message()   │
│  with text string        │
└──────────┬───────────────┘
           │
           ▼
┌──────────────────────────┐
│  Reset Timer             │
│  lv_timer_reset()        │
│  - Restart 5-second timer│
└──────────┬───────────────┘
           │
           ▼
┌──────────────────────────┐
│  Update Label            │
│  lv_label_set_text()     │
│  - Display message       │
│  - Auto-scroll if long   │
└──────────┬───────────────┘
           │
           │ Wait 5 seconds
           │
           ▼
┌──────────────────────────┐
│  Timer Callback          │
│  main_ui_message_        │
│  deleter()               │
│  - Clear label text      │
└──────────────────────────┘
```

---

## Hardware Configuration

### SPI Bus Assignment

```
ESP32 SPI Buses
┌──────────────────────────────────────┐
│  SPI2_HOST (HSPI)                    │
│  ┌────────────────────────────────┐  │
│  │       ST7789 LCD               │  │
│  │  ┌──────────────────────────┐  │  │
│  │  │ SCLK: GPIO 14            │  │  │
│  │  │ MOSI: GPIO 13            │  │  │
│  │  │ MISO: GPIO 12 (unused)   │  │  │
│  │  │ CS:   GPIO 15            │  │  │
│  │  │ DC:   GPIO 2             │  │  │
│  │  │ BL:   GPIO 21            │  │  │
│  │  │ Clock: 40 MHz            │  │  │
│  │  │ DMA: AUTO                │  │  │
│  │  └──────────────────────────┘  │  │
│  └────────────────────────────────┘  │
└──────────────────────────────────────┘

┌──────────────────────────────────────┐
│  SPI3_HOST (VSPI)                    │
│  ┌────────────────────────────────┐  │
│  │      XPT2046 Touch             │  │
│  │  ┌──────────────────────────┐  │  │
│  │  │ SCLK: GPIO 25            │  │  │
│  │  │ MOSI: GPIO 32            │  │  │
│  │  │ MISO: GPIO 39            │  │  │
│  │  │ CS:   GPIO 33            │  │  │
│  │  │ INT:  GPIO 36 (input)    │  │  │
│  │  │ DMA: AUTO                │  │  │
│  │  └──────────────────────────┘  │  │
│  └────────────────────────────────┘  │
└──────────────────────────────────────┘
```

### Memory Architecture

```
┌─────────────────────────────────────────┐
│         ESP32 Memory Map                │
│                                         │
│  ┌───────────────────────────────────┐  │
│  │  Internal Flash                   │  │
│  │  ┌─────────────────────────────┐  │  │
│  │  │ Bootloader                  │  │  │
│  │  ├─────────────────────────────┤  │  │
│  │  │ Partition Table             │  │  │
│  │  ├─────────────────────────────┤  │  │
│  │  │ NVS (WiFi credentials)      │  │  │
│  │  ├─────────────────────────────┤  │  │
│  │  │ Application Code            │  │  │
│  │  │ - main                      │  │  │
│  │  │ - driver_init               │  │  │
│  │  │ - main-ui                   │  │  │
│  │  │ - wifi_handler              │  │  │
│  │  ├─────────────────────────────┤  │  │
│  │  │ LVGL Library (~100KB)       │  │  │
│  │  ├─────────────────────────────┤  │  │
│  │  │ ESP-IDF Framework           │  │  │
│  │  └─────────────────────────────┘  │  │
│  └───────────────────────────────────┘  │
│                                         │
│  ┌───────────────────────────────────┐  │
│  │  Internal SRAM (~520KB)           │  │
│  │  ┌─────────────────────────────┐  │  │
│  │  │ Heap (dynamic allocation)   │  │  │
│  │  │ - UI objects                │  │  │
│  │  │ - Styles                    │  │  │
│  │  │ - Buffers                   │  │  │
│  │  ├─────────────────────────────┤  │  │
│  │  │ Stack                       │  │  │
│  │  │ - app_main_UI_starter: 8KB  │  │  │
│  │  │ - Other tasks               │  │  │
│  │  ├─────────────────────────────┤  │  │
│  │  │ DMA-capable Memory          │  │  │
│  │  │ - LVGL Buffer 1: ~12.8KB    │  │  │
│  │  │ - LVGL Buffer 2: ~12.8KB    │  │  │
│  │  │ Total DMA: ~25.6KB          │  │  │
│  │  ├─────────────────────────────┤  │  │
│  │  │ Global/Static Variables     │  │  │
│  │  │ - Display handles           │  │  │
│  │  │ - Touch handles             │  │  │
│  │  │ - UI object pointers        │  │  │
│  │  └─────────────────────────────┘  │  │
│  └───────────────────────────────────┘  │
└─────────────────────────────────────────┘
```

### FreeRTOS Task Architecture

```
┌──────────────────────────────────────────┐
│        FreeRTOS Scheduler                │
│                                          │
│  ┌────────────────────────────────────┐  │
│  │  app_main_UI_starter               │  │
│  │  Priority: 3                       │  │
│  │  Stack: 8192 bytes                 │  │
│  │  Core: tskNO_AFFINITY              │  │
│  │  ┌──────────────────────────────┐  │  │
│  │  │ Loop every 1ms:              │  │  │
│  │  │  lv_lock()                   │  │  │
│  │  │  lv_timer_handler()          │  │  │
│  │  │  lv_unlock()                 │  │  │
│  │  │  vTaskDelay(1)               │  │  │
│  │  └──────────────────────────────┘  │  │
│  └────────────────────────────────────┘  │
│                                          │
│  ┌────────────────────────────────────┐  │
│  │  ESP-IDF System Tasks              │  │
│  │  ┌──────────────────────────────┐  │  │
│  │  │ WiFi Task                    │  │  │
│  │  │ Priority: 23 (high)          │  │  │
│  │  ├──────────────────────────────┤  │  │
│  │  │ TCP/IP Task (lwIP)           │  │  │
│  │  │ Priority: 18                 │  │  │
│  │  ├──────────────────────────────┤  │  │
│  │  │ Event Loop Task              │  │  │
│  │  │ Priority: 20                 │  │  │
│  │  ├──────────────────────────────┤  │  │
│  │  │ Timer Service Task           │  │  │
│  │  │ Priority: configTIMER_TASK_  │  │  │
│  │  │           PRIORITY           │  │  │
│  │  └──────────────────────────────┘  │  │
│  └────────────────────────────────────┘  │
└──────────────────────────────────────────┘
```

### Timing and Synchronization

```
Time (ms)
   0  ├─ System Boot
      │
  50  ├─ Hardware Init Complete
      │
 100  ├─ UI Task Created
      │
 150  ├─ Main UI Initialized
      │
      │  ┌──────────────────────────────┐
      │  │   Normal Operation           │
      │  └──────────────────────────────┘
      │
      ├─ Every 1ms:
      │  └─ lv_tick_inc(1)
      │     via ESP timer ISR
      │
      ├─ Every ~1ms:
      │  └─ UI Task runs
      │     └─ lv_timer_handler()
      │        ├─ Process timers
      │        ├─ Handle input
      │        └─ Render if needed
      │
      ├─ On Touch:
      │  └─ XPT2046 INT triggered
      │     └─ Next lv_timer_handler()
      │        reads touch data
      │
      ├─ On Render:
      │  └─ flush_cb() called
      │     └─ SPI transfer to LCD
      │        (~5-15ms for full screen)
      │
      ▼
```

---

## Build System

### CMake Project Structure

```
Project Root
│
├── CMakeLists.txt (Root)
│   ├── cmake_minimum_required(3.16)
│   ├── include($ENV{IDF_PATH}/tools/cmake/project.cmake)
│   └── project(LCD-project-st7789)
│
├── main/
│   ├── CMakeLists.txt
│   │   ├── SRCS: hello_world_main.c
│   │   └── REQUIRES: wifi_handler, nvs_flash,
│   │                 driver_init, main-ui
│   └── hello_world_main.c
│
├── components/
│   │
│   ├── driver_init/
│   │   ├── CMakeLists.txt
│   │   │   ├── SRCS: driver_init.c
│   │   │   └── REQUIRES: lvgl, driver, esp_lcd,
│   │   │                 espressif__esp_lcd_touch,
│   │   │                 atanisoft__esp_lcd_touch_xpt2046,
│   │   │                 esp_timer
│   │   ├── driver_init.c
│   │   └── driver_init.h
│   │
│   ├── wifi_handler/
│   │   ├── CMakeLists.txt
│   │   │   ├── SRCS: wifi_handler.c
│   │   │   └── REQUIRES: esp_wifi
│   │   ├── wifi_handler.c
│   │   └── wifi_handler.h
│   │
│   └── main-ui/
│       ├── CMakeLists.txt
│       │   ├── SRCS: main_ui.c, main_event_handlers.c,
│       │   │         wifi_ui.c, UI_commons.c,
│       │   │         tile_uis/*.c
│       │   └── REQUIRES: lvgl, driver_init, wifi_handler
│       ├── main_ui.c
│       ├── main_ui.h
│       ├── main_event_handlers.c
│       ├── wifi_ui.c
│       ├── UI_commons.c
│       └── tile_uis/
│           ├── wifi_tile.c
│           ├── ble_tile.c
│           ├── other_tile.c
│           ├── wifi_icons.c
│           └── bluetooth_icons.c
│
└── managed_components/
    ├── lvgl__lvgl/
    ├── espressif__esp_lcd_touch/
    └── atanisoft__esp_lcd_touch_xpt2046/
```

### Component Dependency Graph

```
                    main
                     │
        ┌────────────┼─────────────┐
        │            │             │
        ▼            ▼             ▼
  wifi_handler  driver_init    main-ui
        │            │             │
        │      ┌─────┼─────┐       │
        │      │     │     │       │
        ▼      ▼     ▼     ▼       ▼
    esp_wifi  lvgl driver esp_lcd  │
               │     │     │       │
               │     │     ▼       │
               │     │  esp_timer  │
               │     │             │
               └─────┴─────────────┘
                     │
                     ▼
              ┌──────────────┐
              │  ESP-IDF     │
              │  Framework   │
              └──────────────┘
```

### Build and Flash Process

```
1. Build Command:
   idf.py build
   │
   ├─ Configure (CMake)
   │  └─ Generate build files
   │
   ├─ Compile
   │  ├─ main
   │  ├─ driver_init
   │  ├─ wifi_handler
   │  ├─ main-ui
   │  └─ managed_components
   │
   ├─ Link
   │  └─ Create ELF file
   │
   └─ Generate Binary
      ├─ .bin file
      ├─ .map file
      └─ Size info

2. Flash Command:
   idf.py -p COM_PORT flash
   │
   ├─ Erase flash (if needed)
   ├─ Write bootloader
   ├─ Write partition table
   ├─ Write application
   └─ Verify

3. Monitor:
   idf.py -p COM_PORT monitor
   │
   └─ Show serial output
      └─ Decode stack traces
```

---

## Key Features & Capabilities

### Current Implementation

✅ **Display Management**
- ST7789 LCD driver (240x320 resolution)
- RGB565 color format
- 40MHz SPI clock
- Hardware-accelerated rendering via LVGL
- Double-buffered drawing (partial refresh)

✅ **Touch Input**
- XPT2046 resistive touch controller
- Coordinate transformation
- Press/release state detection
- LVGL input device integration

✅ **User Interface**
- Tile-based layout system
- WiFi management tile with icon and switch
- Bluetooth management tile
- Utility bar with status icons
- Scrolling message system
- Auto-clearing notifications (5-second timer)
- Grid and flexbox layouts

✅ **WiFi Subsystem**
- Station mode initialization
- Event-driven architecture
- Scan capability (framework in place)
- WPA2/WPA3 support

✅ **Event Handling**
- Switch toggle events
- Button click events
- Callback-based architecture
- User data propagation

### System Performance

**Rendering:**
- Frame rate: ~30-60 FPS (depending on complexity)
- Partial screen updates minimize SPI traffic
- DMA transfers reduce CPU overhead

**Memory Usage:**
- LVGL buffers: ~25.6 KB (DMA memory)
- UI task stack: 8 KB
- Heap usage: Dynamic (objects, styles)

**Power Consumption:**
- Active display: ~100-150mA (estimated)
- Backlight: Major power consumer
- WiFi active: +80-120mA (estimated)

---

## Development Notes

### Code Organization Principles

1. **Modular Design**
   - Each component in separate directory
   - Clear public/private interfaces
   - Minimal inter-component dependencies

2. **LVGL Integration**
   - Uses LVGL v9.x features
   - Thread-safe access via lv_lock/unlock
   - Efficient memory management

3. **Event-Driven Architecture**
   - ESP-IDF event loop for system events
   - LVGL event system for UI interactions
   - Callback-based communication

4. **Memory Management**
   - Dynamic allocation for LVGL objects
   - DMA-capable memory for graphics
   - Proper cleanup patterns

### Common Patterns

**Object Creation Pattern:**
```c
// Define
DEFINE_OBJECT(my_object);
DEFINE_STYLE(my_object);

// Create
object_my_object = lv_obj_create(parent);
LV_ASSERT(object_my_object);

// Style
MALLOC_STYLE(style_my_object);
lv_style_init(style_my_object);
lv_style_set_*(...);
lv_obj_add_style(object_my_object, style_my_object, LV_PART_MAIN);
```

**Event Handler Pattern:**
```c
void my_event_handler(lv_event_t *event) {
    lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t *target = lv_event_get_current_target(event);
    void *user_data = lv_event_get_user_data(event);

    // Handle event
}

// Register
lv_obj_add_event_cb(object, my_event_handler,
                    LV_EVENT_CLICKED, user_data);
```

---

## Troubleshooting Guide

### Display Issues

**Problem: Display not working**
- Check SPI2 pin connections (14, 13, 15, 2)
- Verify backlight pin (GPIO 21) is HIGH
- Check 3.3V power supply
- Verify SPI clock speed (40MHz max)

**Problem: Display garbled**
- Check RGB byte order swap in flush_cb()
- Verify display orientation settings
- Check gap settings (0, 0 for most ST7789)

### Touch Issues

**Problem: Touch not responding**
- Check SPI3 pin connections (25, 32, 39, 33, 36)
- Verify interrupt pin (GPIO 36) connectivity
- Check touch calibration in XPT2046 config
- Test with different pressure levels

**Problem: Touch coordinates inverted**
- Adjust transformation in lvgl_touch_cb()
- Current: x = 239 - raw_x, y = raw_y
- Modify based on your display orientation

### WiFi Issues

**Problem: WiFi not starting**
- Check NVS partition initialization
- Verify event loop creation
- Review WiFi credentials
- Check antenna connection

### Memory Issues

**Problem: Heap corruption / Stack overflow**
- Increase UI task stack size (currently 8KB)
- Check for memory leaks in UI code
- Monitor heap usage with ESP_LOG
- Use heap tracing tools

---

## Future Enhancements

### Suggested Improvements

1. **WiFi Features**
   - Complete WiFi scan UI
   - Network selection list
   - Password input keyboard
   - Connection status display
   - Signal strength indicator

2. **Bluetooth Features**
   - BLE initialization
   - Device discovery
   - Pairing interface
   - Connection management

3. **UI Enhancements**
   - Themes (dark/light mode)
   - Animations and transitions
   - Custom fonts
   - More interactive widgets
   - Screen saver

4. **System Features**
   - Settings persistence to NVS
   - Power management
   - Backlight control (PWM dimming)
   - Sleep modes
   - OTA updates

5. **Performance**
   - Use hardware acceleration (if available)
   - Optimize rendering pipeline
   - Reduce memory footprint
   - Battery monitoring

---

## Conclusion

This LCD-ST7789 project demonstrates a well-architected embedded system with:

- **Clean separation of concerns** between hardware drivers, UI, and business logic
- **Efficient use of LVGL** for modern, responsive graphics
- **Event-driven design** for scalability and maintainability
- **FreeRTOS integration** for multitasking
- **Modular component structure** for easy extension

The codebase provides a solid foundation for building more complex IoT devices with touchscreen interfaces.

---

**Document Version:** 1.0
**Created:** 2025-12-16
**Target Platform:** ESP32 with ESP-IDF v5.4.x
**Display:** ST7789 240x320 LCD
**Author:** Generated Project Documentation
