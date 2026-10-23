idf_component_register(
    SRCS
        "main.c"
        "board/display.cpp"
        "board/audio.c"
        "board/clock.c"
        "ui/desk_ui.c"
        "ui/island_face.c"
    INCLUDE_DIRS "." "board" "ui"
    REQUIRES lvgl__lvgl esphome__esp-hub75 espressif__esp_lvgl_port
             espressif__esp_codec_dev esp_driver_gpio esp_driver_i2c
             esp_driver_i2s esp_driver_usb_serial_jtag esp_timer nvs_flash vfs
)
