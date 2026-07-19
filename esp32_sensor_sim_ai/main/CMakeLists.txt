idf_component_register(
    SRCS "main.c" "sensor_protocol.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_event nvs_flash mqtt esp_driver_uart esp_netif esp_http_server
)
