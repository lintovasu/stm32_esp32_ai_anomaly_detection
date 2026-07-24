#ifndef OTA_RECEIVER_H
#define OTA_RECEIVER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Runs inside the APPLICATION, not the bootloader. Consumes HDLC-framed
 * bytes from the ESP32-S3 UART link (reuses the same byte-stuffing +
 * CRC16 framing already used for sensor telemetry) and writes the
 * incoming firmware image to the currently-inactive slot.
 *
 * Frame payloads (after HDLC unframing/CRC16 check) are one of:
 *   OTA_CMD_BEGIN  { total_size: u32 }
 *   OTA_CMD_DATA   { offset: u32, len: u16, data: bytes }
 *   OTA_CMD_END    { crc32: u32 }
 * See esp32_host/ota_uart_protocol.md for the exact wire format and the
 * ESP32-side state machine that drives this.
 */

typedef enum {
    OTA_CMD_BEGIN = 0x01,
    OTA_CMD_DATA  = 0x02,
    OTA_CMD_END   = 0x03,
} ota_cmd_t;

typedef enum {
    OTA_OK = 0,
    OTA_ERR_NOT_STARTED,
    OTA_ERR_ALREADY_STARTED,
    OTA_ERR_TOO_LARGE,
    OTA_ERR_ERASE_FAILED,
    OTA_ERR_WRITE_FAILED,
    OTA_ERR_BAD_OFFSET,
    OTA_ERR_CRC_MISMATCH,
    OTA_ERR_METADATA_WRITE_FAILED,
} ota_result_t;

/* Call once at startup so the module knows where the staging slot is
 * (always "the slot that is not currently active"). */
void ota_receiver_init(void);

/* Call when an OTA_CMD_BEGIN frame arrives. Erases the staging slot
 * (this is the ~1s-per-128KB-sector blocking operation -- see design
 * notes on doing this from a low-priority FreeRTOS task, not an ISR). */
ota_result_t ota_receiver_begin(uint32_t total_size);

/* Call for each OTA_CMD_DATA frame. Writes `len` bytes at `offset`
 * within the staging slot. Offsets must arrive in order in this simple
 * version (no out-of-order/resume support -- see notes in .c file for
 * how you'd extend it). */
ota_result_t ota_receiver_data(uint32_t offset, const uint8_t *data, uint16_t len);

/* Call when an OTA_CMD_END frame arrives with the ESP32's computed
 * CRC32. Recomputes CRC32 over the staged image and, if it matches,
 * writes the metadata (staging_size, staging_crc32, pending_update=1)
 * so the bootloader picks it up on next reset. Does NOT reset the MCU
 * itself -- caller decides when (e.g. after acking the ESP32 so it
 * knows the transfer succeeded). */
ota_result_t ota_receiver_end(uint32_t expected_crc32);

#endif /* OTA_RECEIVER_H */
