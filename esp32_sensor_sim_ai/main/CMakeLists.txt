idf_component_register(
    SRCS "main.c" "sensor_protocol.c" "ota_apply.c" "ota_crc32.c" "ota_push.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_event nvs_flash mqtt esp_driver_uart esp_netif esp_http_server esp_http_client
)
