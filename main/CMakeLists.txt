idf_component_register(
    SRCS 
        "hello_world_main.c"
    INCLUDE_DIRS 
        "."
    REQUIRES 
        wifi_handler 
        nvs_flash 
        driver_init 
        main-ui
        heap_monitor  # Add this line
)