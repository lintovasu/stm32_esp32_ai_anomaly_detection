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
#include <stdlib.h>
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
#include "ota_push.h"
#include "ota_apply.h"

#define WIFI_SSID       "Vodafone-AF70"
#define WIFI_PASS       "phBmMpGdmr4gFTdm"
#define MQTT_BROKER_URI "mqtt://broker.hivemq.com:1883"
#define MQTT_TOPIC_DATA  "linto/sensors/node1/data"   // make unique to you
#define MQTT_TOPIC_DIAG  "linto/sensors/node1/diag"
#define MQTT_TOPIC_ALERT "linto/sensors/node1/alert"
#define MQTT_TOPIC_OTA   "linto/sensors/node1/ota/update"  // operator publishes here to trigger an update
#define MQTT_TOPIC_OTA_STATUS "linto/sensors/node1/ota/status"  // OTA progress/status messages (JSON)

#define UART_PORT       UART_NUM_1
#define UART_RX_PIN     17
#define UART_TX_PIN     18
#define UART_BUF_SIZE   256

static const char *TAG = "sensor_bridge";

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

esp_mqtt_client_handle_t s_mqtt_client;
volatile bool s_mqtt_connected = false;

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

/* ---------------- OTA trigger (MQTT -> apply task) ---------------- */

/* Heap-allocated so it survives past the MQTT event callback that
 * creates it -- the callback's own event_data buffer is transient and
 * must not be referenced after the handler returns. Freed by the task
 * itself once done. */
typedef struct {
    char manifest_url[256];
    char slot[2];
} ota_trigger_params_t;

static void ota_apply_task(void *pvParameters)
{
    ota_trigger_params_t *params = (ota_trigger_params_t *)pvParameters;

    ESP_LOGI(TAG, "OTA triggered: manifest=%s slot=%s", params->manifest_url, params->slot);
    bool ok = ota_apply_from_manifest(params->manifest_url, params->slot);
    ESP_LOGI(TAG, "OTA %s", ok ? "SUCCEEDED" : "FAILED");

    free(params);
    vTaskDelete(NULL);
}

/* Expects a JSON payload like:
 *   {"manifest_url": "https://.../releases/download/v1.4.0/manifest.json", "slot": "B"}
 * 'slot' should be whichever slot the STM32 is currently NOT running --
 * see the earlier discussion on tracking that via device status
 * reporting; for now this is operator-supplied. */
static void handle_ota_trigger(const char *data, int data_len)
{
    char *json_str = malloc(data_len + 1);
    if (!json_str) return;
    memcpy(json_str, data, data_len);
    json_str[data_len] = '\0';

    cJSON *root = cJSON_Parse(json_str);
    free(json_str);
    if (!root) {
        ESP_LOGE(TAG, "OTA trigger: invalid JSON");
        return;
    }

    cJSON *url_item  = cJSON_GetObjectItem(root, "manifest_url");
    cJSON *slot_item = cJSON_GetObjectItem(root, "slot");

    if (!cJSON_IsString(url_item) || !cJSON_IsString(slot_item)) {
        ESP_LOGE(TAG, "OTA trigger: missing manifest_url or slot");
        cJSON_Delete(root);
        return;
    }

    ota_trigger_params_t *params = malloc(sizeof(ota_trigger_params_t));
    if (!params) {
        cJSON_Delete(root);
        return;
    }
    strncpy(params->manifest_url, url_item->valuestring, sizeof(params->manifest_url) - 1);
    params->manifest_url[sizeof(params->manifest_url) - 1] = '\0';
    strncpy(params->slot, slot_item->valuestring, sizeof(params->slot) - 1);
    params->slot[sizeof(params->slot) - 1] = '\0';

    cJSON_Delete(root);

    /* Larger stack than the default -- HTTP client + TLS + mbedtls
     * SHA-256 use a fair amount. Runs on its own task so the download
     * (potentially tens of seconds) never blocks the MQTT client task
     * or uart_rx_task. */
    xTaskCreate(ota_apply_task, "ota_apply_task", 8192, params, 5, NULL);
}

/* ---------------- MQTT ---------------- */

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected to broker");
            s_mqtt_connected = true;
            esp_mqtt_client_subscribe(s_mqtt_client, MQTT_TOPIC_OTA, 1);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT disconnected");
            s_mqtt_connected = false;
            break;
        case MQTT_EVENT_DATA:
            if (event->topic_len == (int)strlen(MQTT_TOPIC_OTA) &&
                strncmp(event->topic, MQTT_TOPIC_OTA, event->topic_len) == 0) {
                handle_ota_trigger(event->data, event->data_len);
            }
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
    uint8_t payload[sizeof(decoder.buf)];
    uint16_t payload_len;
    SensorFrame_t frame;

    TickType_t last_diag_publish = xTaskGetTickCount();

    for (;;) {
        int len = uart_read_bytes(UART_PORT, data, UART_BUF_SIZE, pdMS_TO_TICKS(200));
        for (int i = 0; i < len; i++) {
            hdlc_result_t result = hdlc_feed_byte(&decoder, data[i],
                                                   payload, sizeof(payload), &payload_len);
            switch (result) {
                case HDLC_FRAME_COMPLETE:
                    if (payload_len == SENSOR_FRAME_SIZE) {
                        s_frames_ok++;
                        memcpy(&frame, payload, SENSOR_FRAME_SIZE);
                        publish_sensor_frame(&frame);
                    } else if (payload_len == 1) {
                        /* OTA ACK/NAK response from the STM32 -- hand
                         * it to whichever ota_push transfer is
                         * currently waiting for one. */
                        ESP_LOGW(TAG, "OTA ACK/NAK response length = %u", payload_len);
                        ota_push_on_ack_byte(payload[0]);
                    } else {
                        ESP_LOGW(TAG, "unexpected frame length %u, discarding", payload_len);
                    }
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
    ota_push_init();
    wifi_init_sta();
    mqtt_init();

    xTaskCreate(uart_rx_task, "uart_rx_task", 4096, NULL, 5, NULL);
}
