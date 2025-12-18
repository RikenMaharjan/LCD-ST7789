idf_component_register(
    SRCS 
        "heap_monitor.c"
    INCLUDE_DIRS 
        "."
    REQUIRES 
        esp_timer
        freertos
        heap
        log
    PRIV_REQUIRES
        # Add lvgl here if you want LVGL memory monitoring
        # lvgl
)