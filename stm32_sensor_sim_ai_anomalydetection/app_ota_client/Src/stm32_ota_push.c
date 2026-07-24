/* Reference implementation for the ESP32-S3 side (ESP-IDF).
 * Assumes:
 *   - `stm32_send_frame()` / `stm32_wait_ack()` already exist as part of
 *     your existing HDLC+CRC16 UART layer (used today for telemetry).
 *   - The downloaded firmware image is available as a buffer or can be
 *     streamed via a read callback (shown here as a simple buffer for
 *     clarity -- swap for streaming from esp_https_ota's file handle if
 *     the image is too large to hold fully in RAM alongside everything
 *     else running on the ESP32-S3).
 *
 * This file intentionally does NOT include the MQTT/esp_https_ota
 * download logic -- that's the "ESP32 native OTA" half you already have
 * a path for (esp_https_ota component). This file is just the piece
 * that pushes an already-downloaded, already-verified image onward to
 * the STM32.
 */

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_log.h"
#include "esp_crc.h" /* ESP-IDF's CRC32, matches zlib/STM32 crc32.c */

#define CHUNK_SIZE 512u

static const char *TAG = "stm32_ota_push";

#define OTA_CMD_BEGIN 0x01
#define OTA_CMD_DATA  0x02
#define OTA_CMD_END   0x03

/* These two are stand-ins for your existing HDLC frame layer -- adapt
 * to whatever your current telemetry code actually calls. */
extern bool stm32_send_frame(const uint8_t *payload, uint16_t len);
extern bool stm32_wait_ack(uint32_t timeout_ms);

static bool send_begin(uint32_t total_size)
{
    uint8_t buf[5];
    buf[0] = OTA_CMD_BEGIN;
    memcpy(&buf[1], &total_size, sizeof(uint32_t));

    if (!stm32_send_frame(buf, sizeof(buf))) return false;
    return stm32_wait_ack(2000); /* erase can take a couple seconds */
}

static bool send_data(uint32_t offset, const uint8_t *data, uint16_t len)
{
    uint8_t buf[1 + 4 + 2 + CHUNK_SIZE];
    uint16_t pos = 0;

    buf[pos++] = OTA_CMD_DATA;
    memcpy(&buf[pos], &offset, sizeof(uint32_t)); pos += 4;
    memcpy(&buf[pos], &len, sizeof(uint16_t));    pos += 2;
    memcpy(&buf[pos], data, len);                 pos += len;

    if (!stm32_send_frame(buf, pos)) return false;
    return stm32_wait_ack(500);
}

static bool send_end(uint32_t crc32)
{
    uint8_t buf[5];
    buf[0] = OTA_CMD_END;
    memcpy(&buf[1], &crc32, sizeof(uint32_t));

    if (!stm32_send_frame(buf, sizeof(buf))) return false;
    return stm32_wait_ack(1000);
}

/* `image` must already be verified (SHA256 checked against what the
 * MQTT message promised) before calling this. Returns true only if the
 * STM32 confirmed the full image was written and matched CRC32. */
bool stm32_ota_push(const uint8_t *image, uint32_t total_size)
{
    if (!send_begin(total_size)) {
        ESP_LOGE(TAG, "BEGIN rejected or no ACK");
        return false;
    }

    uint32_t crc = esp_crc32_le(0, image, total_size);
    /* esp_crc32_le already applies the same init/final XOR convention
     * as the STM32 side's crc32_calc() -- verify this against your
     * IDF version's docs; if it differs, wrap it to match
     * bootloader/crc32.c's exact algorithm so both sides agree. */

    uint32_t offset = 0;
    const int MAX_RETRIES = 3;

    while (offset < total_size) {
        uint16_t len = (total_size - offset > CHUNK_SIZE)
                            ? CHUNK_SIZE
                            : (uint16_t)(total_size - offset);

        int attempt = 0;
        bool ok = false;
        while (attempt < MAX_RETRIES && !ok) {
            ok = send_data(offset, image + offset, len);
            attempt++;
            if (!ok) {
                ESP_LOGW(TAG, "chunk at offset %u failed, retry %d", offset, attempt);
            }
        }

        if (!ok) {
            ESP_LOGE(TAG, "chunk at offset %u failed after %d retries, aborting", offset, MAX_RETRIES);
            return false;
        }

        offset += len;
    }

    if (!send_end(crc)) {
        ESP_LOGE(TAG, "END rejected -- CRC mismatch or metadata write failed on STM32");
        return false;
    }

    ESP_LOGI(TAG, "OTA image delivered and staged on STM32 (%u bytes)", total_size);
    return true;
}
