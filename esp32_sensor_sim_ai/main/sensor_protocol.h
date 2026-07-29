#ifndef SENSOR_PROTOCOL_H
#define SENSOR_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Must match stm32_sensor_sim/Core/Inc/sensor_protocol.h exactly */

#define HDLC_FLAG    0x7E
#define HDLC_ESC     0x7D
#define HDLC_ESC_XOR 0x20

#pragma pack(push, 1)
typedef struct {
    uint8_t  sensor_id;
    uint32_t timestamp_ms;
    float    temperature_c;
    float    vibration_rms_g;
    float    pressure_kpa;
    float    anomaly_score;
    uint8_t  is_anomaly;
} SensorFrame_t;
#pragma pack(pop)

#define SENSOR_FRAME_SIZE sizeof(SensorFrame_t)

/* ============================================================
 * Decoder buffer sizing.
 *
 * The same UART link now carries two different frame types:
 *   - sensor telemetry frames (fixed SENSOR_FRAME_SIZE bytes)
 *   - OTA ACK/NAK responses from the STM32 (1 byte payload)
 *
 * so the decoder buffer needs to be large enough for whichever is
 * bigger. It's also reused to encode OUTGOING OTA command frames
 * (BEGIN/DATA/END) toward the STM32, where DATA chunks can carry up
 * to OTA_CHUNK_SIZE bytes of firmware -- that's the actual largest
 * payload on this link in either direction.
 * ============================================================ */
#define OTA_CHUNK_SIZE 512
#define HDLC_MAX_PAYLOAD (1 + 4 + 2 + OTA_CHUNK_SIZE)  /* cmd+offset+len+data */

/* Decoder holds accumulated de-stuffed payload+crc bytes for one frame.
 * Now generic/variable-length -- NOT tied to SensorFrame_t size, so it
 * can decode both sensor telemetry frames and short OTA ACK/NAK
 * frames arriving on the same stream. */
typedef struct {
    uint8_t  buf[HDLC_MAX_PAYLOAD + 2]; /* payload + 2 CRC bytes, worst case */
    uint16_t idx;
    bool     in_frame;
    bool     esc_pending;
} hdlc_decoder_t;

typedef enum {
    HDLC_NEED_MORE_DATA,
    HDLC_FRAME_COMPLETE,
    HDLC_FRAME_CRC_ERROR,
    HDLC_FRAME_LENGTH_ERROR,   /* now only means "overflowed the buffer" */
} hdlc_result_t;

uint16_t crc16_ccitt(const uint8_t *data, size_t len);

/* Feed one raw byte from UART into the decoder. On HDLC_FRAME_COMPLETE,
 * the decoded, CRC-verified payload is in out_buf[0 .. *out_len - 1] --
 * caller decides what it is based on the length: SENSOR_FRAME_SIZE
 * means sensor telemetry (memcpy into a SensorFrame_t), 1 byte means
 * an OTA ACK/NAK response, anything else is unexpected. */
hdlc_result_t hdlc_feed_byte(hdlc_decoder_t *dec, uint8_t byte,
                              uint8_t *out_buf, uint16_t out_buf_size,
                              uint16_t *out_len);

/* Encodes an arbitrary payload with HDLC framing + byte stuffing --
 * used for outgoing OTA command frames (BEGIN/DATA/END) toward the
 * STM32. Returns bytes written to 'out', or 0 on overflow. */
uint16_t hdlc_encode_raw(const uint8_t *payload, uint16_t payload_len,
                          uint8_t *out, uint16_t out_size);

/* ============================================================
 * OTA command/response byte values -- must match STM32
 * app_ota_client/inc/ota_receiver.h exactly.
 * ============================================================ */
#define OTA_CMD_BEGIN 0x01
#define OTA_CMD_DATA  0x02
#define OTA_CMD_END   0x03

#define OTA_ACK_BYTE 0xA5
#define OTA_NAK_BYTE 0x5A

#endif /* SENSOR_PROTOCOL_H */
