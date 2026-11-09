if(NOT CMAKE_BUILD_EARLY_EXPANSION)
file(GLOB dashboard_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../preview/*.js" "${CMAKE_CURRENT_SOURCE_DIR}/../preview/*.css" "${CMAKE_CURRENT_SOURCE_DIR}/../preview/index.html")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${dashboard_sources} "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/pack_dashboard.py" "${CMAKE_CURRENT_SOURCE_DIR}/../cellular-automata-config.json")
idf_build_get_property(dashboard_python PYTHON)
execute_process(COMMAND "${dashboard_python}" "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/pack_dashboard.py" "${CMAKE_CURRENT_SOURCE_DIR}/../preview" "${CMAKE_CURRENT_BINARY_DIR}/dashboard_assets.h" COMMAND_ERROR_IS_FATAL ANY)
endif()
idf_component_register(
    SRCS
        "main.c"
        "board/display.cpp"
        "board/audio.c"
        "board/clock.c"
        "board/network.c"
        "board/weather.c"
        "board/dashboard.c"
        "ui/weather_layer.c"
        "ui/desk_ui.c"
        "ui/cellular_automata.c"
        "ui/island_face.c"
        "ui/minute_birds.c"
        "ui/scene_player.c"
        "ui/scene_store.c"
    INCLUDE_DIRS "." "board" "ui"
    REQUIRES lvgl__lvgl esphome__esp-hub75 espressif__esp_lvgl_port
             espressif__esp_codec_dev esp_driver_gpio esp_driver_i2c
             esp_driver_i2s esp_driver_usb_serial_jtag esp_timer esp_partition nvs_flash vfs
             esp_wifi esp_event esp_netif lwip esp_http_client esp_http_server mbedtls json
)
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
 target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
endif()
