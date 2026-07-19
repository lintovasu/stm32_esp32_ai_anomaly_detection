/* ============================================================
 * ESP32-S3 Sensor Bridge: UART (HDLC framed) -> WiFi -> MQTT
 * ============================================================
 * Receives HDLC-framed, CRC16-protected sensor packets from the
 * STM32 simulator, decodes them, converts to JSON, and publishes
 * to MQTT. Tracks frame/CRC error counters and publishes a
 * periodic diagnostics message too - useful once you move from
 * simulated data to a real UART link with actual noise.
 *
 * Wiring:
 *   ESP32-S3 RX (GPIO17) <- STM32 TX
 *   ESP32-S3 TX (GPIO18) -> STM32 RX
 *   ESP32-S3 GND         <- STM32 GND
 * ============================================================ */

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/uart.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include "sensor_protocol.h"

#define WIFI_SSID       "Vodafone-AF70"
#define WIFI_PASS       "phBmMpGdmr4gFTdm"
#define MQTT_BROKER_URI "mqtt://broker.hivemq.com:1883"
#define MQTT_TOPIC_DATA  "linto/sensors/node1/data"   // make unique to you
#define MQTT_TOPIC_DIAG  "linto/sensors/node1/diag"
#define MQTT_TOPIC_ALERT "linto/sensors/node1/alert"

#define UART_PORT       UART_NUM_1
#define UART_RX_PIN     17
#define UART_TX_PIN     18
#define UART_BUF_SIZE   256

static const char *TAG = "sensor_bridge";

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static esp_mqtt_client_handle_t s_mqtt_client;
static volatile bool s_mqtt_connected = false;

/* Diagnostics counters, published periodically */
static volatile uint32_t s_frames_ok = 0;
static volatile uint32_t s_frames_crc_err = 0;
static volatile uint32_t s_frames_len_err = 0;

/* ---------------- WiFi ---------------- */

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi disconnected, retrying...");
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to WiFi SSID: %s ...", WIFI_SSID);
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(TAG, "WiFi connected.");
}

/* ---------------- MQTT ---------------- */

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected to broker");
            s_mqtt_connected = true;
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT disconnected");
            s_mqtt_connected = false;
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error event");
            break;
        default:
            break;
    }
}

static void mqtt_init(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
    };
    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_mqtt_client);
}

/* ---------------- UART + HDLC decode + publish ---------------- */

static void uart_init(void)
{
    uart_config_t cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN,
                                  UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, UART_BUF_SIZE * 2, 0, 0, NULL, 0));
}

static void publish_sensor_frame(const SensorFrame_t *frame)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "sensor_id", frame->sensor_id);
    cJSON_AddNumberToObject(root, "ts_ms", frame->timestamp_ms);
    cJSON_AddNumberToObject(root, "temperature_c", frame->temperature_c);
    cJSON_AddNumberToObject(root, "vibration_rms_g", frame->vibration_rms_g);
    cJSON_AddNumberToObject(root, "pressure_kpa", frame->pressure_kpa);
    cJSON_AddNumberToObject(root, "anomaly_score", frame->anomaly_score);
    cJSON_AddBoolToObject(root, "is_anomaly", frame->is_anomaly ? true : false);

    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str != NULL) {
        if (s_mqtt_connected) {
            esp_mqtt_client_publish(s_mqtt_client, MQTT_TOPIC_DATA, json_str, 0, 1, 0);
            ESP_LOGI(TAG, "Published: %s", json_str);

            /* Fire a dedicated alert message too - lets a dashboard or
             * notification service subscribe to just this narrow topic
             * instead of filtering the full data stream client-side. */
            if (frame->is_anomaly) {
                esp_mqtt_client_publish(s_mqtt_client, MQTT_TOPIC_ALERT, json_str, 0, 1, 0);
                ESP_LOGW(TAG, "ANOMALY flagged (score=%.4f)", frame->anomaly_score);
            }
        } else {
            ESP_LOGW(TAG, "MQTT not connected, dropping frame");
        }
        cJSON_free(json_str);
    }
    cJSON_Delete(root);
}

static void publish_diagnostics(void)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "frames_ok", s_frames_ok);
    cJSON_AddNumberToObject(root, "frames_crc_err", s_frames_crc_err);
    cJSON_AddNumberToObject(root, "frames_len_err", s_frames_len_err);

    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str != NULL) {
        if (s_mqtt_connected) {
            esp_mqtt_client_publish(s_mqtt_client, MQTT_TOPIC_DIAG, json_str, 0, 0, 0);
        }
        cJSON_free(json_str);
    }
    cJSON_Delete(root);
}

static void uart_rx_task(void *arg)
{
    uint8_t data[UART_BUF_SIZE];
    hdlc_decoder_t decoder = {0};
    SensorFrame_t frame;

    TickType_t last_diag_publish = xTaskGetTickCount();

    for (;;) {
        int len = uart_read_bytes(UART_PORT, data, UART_BUF_SIZE, pdMS_TO_TICKS(200));
        for (int i = 0; i < len; i++) {
            hdlc_result_t result = hdlc_feed_byte(&decoder, data[i], &frame);
            switch (result) {
                case HDLC_FRAME_COMPLETE:
                    s_frames_ok++;
                    publish_sensor_frame(&frame);
                    break;
                case HDLC_FRAME_CRC_ERROR:
                    s_frames_crc_err++;
                    ESP_LOGW(TAG, "CRC error on received frame (count=%lu)",
                             (unsigned long)s_frames_crc_err);
                    break;
                case HDLC_FRAME_LENGTH_ERROR:
                    s_frames_len_err++;
                    ESP_LOGW(TAG, "Length error on received frame (count=%lu)",
                             (unsigned long)s_frames_len_err);
                    break;
                case HDLC_NEED_MORE_DATA:
                default:
                    break;
            }
        }

        /* Publish diagnostics every ~10 seconds */
        if ((xTaskGetTickCount() - last_diag_publish) > pdMS_TO_TICKS(10000)) {
            publish_diagnostics();
            last_diag_publish = xTaskGetTickCount();
        }
    }
}

/* ---------------- Entry point ---------------- */

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    uart_init();
    wifi_init_sta();
    mqtt_init();

    xTaskCreate(uart_rx_task, "uart_rx_task", 4096, NULL, 5, NULL);
}
