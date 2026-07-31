#include "ota_push.h"
#include "sensor_protocol.h"
#include "ota_crc32.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "esp_log.h"

static const char *TAG = "ota_push";

#define UART_PORT UART_NUM_1  /* must match main.c's UART_PORT */

static QueueHandle_t s_ota_ack_queue;

void ota_push_init(void)
{
    /* Small queue -- only one ACK/NAK is ever pending at a time, since
     * stm32_ota_push() sends one frame and waits before sending the
     * next. Depth of 4 just gives a little slack. */
    s_ota_ack_queue = xQueueCreate(4, sizeof(uint8_t));
}

void ota_push_on_ack_byte(uint8_t byte)
{
    if (s_ota_ack_queue != NULL) {
        /* Non-blocking send from uart_rx_task -- if the queue is full
         * (shouldn't happen given the depth above and one-at-a-time
         * protocol) drop rather than block the RX task. */
        xQueueSend(s_ota_ack_queue, &byte, 0);
    }
}

/* Waits up to timeout_ms for an ACK/NAK byte. Returns true only for
 * OTA_ACK_BYTE; false for OTA_NAK_BYTE, timeout, or an unexpected
 * value. */
static bool wait_for_ack(uint32_t timeout_ms)
{
    uint8_t resp;
    if (xQueueReceive(s_ota_ack_queue, &resp, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        ESP_LOGW(TAG, "timed out waiting for ACK/NAK");
        return false;
    }
    if (resp == OTA_ACK_BYTE) return true;
    if (resp == OTA_NAK_BYTE) {
        ESP_LOGW(TAG, "received NAK");
        return false;
    }
    ESP_LOGW(TAG, "unexpected response byte 0x%02X", resp);
    return false;
}

static bool send_frame_and_wait(const uint8_t *payload, uint16_t len, uint32_t timeout_ms)
{
    uint8_t framebuf[HDLC_MAX_PAYLOAD * 2 + 8]; /* worst-case stuffing + flags */
    uint16_t framelen = hdlc_encode_raw(payload, len, framebuf, sizeof(framebuf));
    if (framelen == 0) {
        ESP_LOGE(TAG, "hdlc_encode_raw overflow, len=%u", len);
        return false;
    }

    int written = uart_write_bytes(UART_PORT, (const char *)framebuf, framelen);
    if (written != framelen) {
        ESP_LOGE(TAG, "uart_write_bytes short write: %d/%u", written, framelen);
        return false;
    }

    return wait_for_ack(timeout_ms);
}

static bool send_begin(uint32_t total_size)
{
    uint8_t buf[5];
    buf[0] = OTA_CMD_BEGIN;
    memcpy(&buf[1], &total_size, sizeof(uint32_t));
    /* Much longer timeout than DATA/END -- BEGIN triggers a full-slot
     * erase on the STM32 side (4 sectors: one 64KB + three 128KB for
     * Slot A, four 128KB for Slot B), which realistically takes
     * several seconds total, not the ~1s a single sector would take.
     * The original 3000ms here was sized for one sector's erase time,
     * not the whole slot -- too short, causing the STM32's genuinely
     * valid ACK to arrive after this wait had already given up. */
    return send_frame_and_wait(buf, sizeof(buf), 20000);
}

static bool send_data(uint32_t offset, const uint8_t *data, uint16_t len)
{
    uint8_t buf[1 + 4 + 2 + OTA_CHUNK_SIZE];
    uint16_t pos = 0;
    buf[pos++] = OTA_CMD_DATA;
    memcpy(&buf[pos], &offset, sizeof(uint32_t)); pos += 4;
    memcpy(&buf[pos], &len, sizeof(uint16_t));    pos += 2;
    memcpy(&buf[pos], data, len);                 pos += len;

    return send_frame_and_wait(buf, pos, 1000);
}

static bool send_end(uint32_t crc32_val)
{
    uint8_t buf[5];
    buf[0] = OTA_CMD_END;
    memcpy(&buf[1], &crc32_val, sizeof(uint32_t));
    return send_frame_and_wait(buf, sizeof(buf), 2000);
}

bool stm32_ota_push(const uint8_t *image, uint32_t total_size)
{
    if (s_ota_ack_queue == NULL) {
        ESP_LOGE(TAG, "ota_push_init() was never called");
        return false;
    }

    /* Drain any stale bytes left in the queue from a previous
     * (possibly failed) transfer before starting a new one. */
    uint8_t discard;
    while (xQueueReceive(s_ota_ack_queue, &discard, 0) == pdTRUE) { }

    ESP_LOGI(TAG, "starting OTA push, %lu bytes", (unsigned long)total_size);

    if (!send_begin(total_size)) {
        ESP_LOGE(TAG, "BEGIN rejected or no ACK");
        return false;
    }

    uint32_t crc = ota_crc32(image, total_size);

    uint32_t offset = 0;
    const int MAX_RETRIES = 3;

    while (offset < total_size) {
        uint16_t len = (total_size - offset > OTA_CHUNK_SIZE)
                            ? OTA_CHUNK_SIZE
                            : (uint16_t)(total_size - offset);

        bool ok = false;
        for (int attempt = 0; attempt < MAX_RETRIES && !ok; attempt++) {
            ok = send_data(offset, image + offset, len);
            if (!ok) {
                ESP_LOGW(TAG, "chunk at offset %lu failed, retry %d/%d",
                         (unsigned long)offset, attempt + 1, MAX_RETRIES);
            }
        }

        if (!ok) {
            ESP_LOGE(TAG, "chunk at offset %lu failed after %d retries, aborting",
                     (unsigned long)offset, MAX_RETRIES);
            return false;
        }

        offset += len;
    }

    if (!send_end(crc)) {
        ESP_LOGE(TAG, "END rejected -- CRC mismatch or metadata write failed on STM32");
        return false;
    }

    ESP_LOGI(TAG, "OTA push complete, STM32 will reboot to apply");
    return true;
}
