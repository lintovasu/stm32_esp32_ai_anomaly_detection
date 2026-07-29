#ifndef SENSOR_PROTOCOL_H
#define SENSOR_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

/* ============================================================
 * Frame format (matches esp32_sensor_sim/main/sensor_protocol.h
 * exactly - both sides must agree byte-for-byte)
 *
 *   FLAG | stuffed(payload) | stuffed(CRC_hi) | stuffed(CRC_lo) | FLAG
 *
 * payload = packed SensorFrame_t (13 bytes)
 * CRC16-CCITT (poly 0x1021, init 0xFFFF) computed over the
 * UN-stuffed payload bytes only.
 * ============================================================ */

#define HDLC_FLAG   0x7E
#define HDLC_ESC    0x7D
#define HDLC_ESC_XOR 0x20

#pragma pack(push, 1)
typedef struct {
    uint8_t  sensor_id;        /* which simulated sensor node, e.g. 0x01 */
    uint32_t timestamp_ms;     /* HAL_GetTick() at time of sample */
    float    temperature_c;
    float    vibration_rms_g;
    float    pressure_kpa;
    float    anomaly_score;    /* autoencoder reconstruction MSE, 0 until
                                   the first full window has been filled */
    uint8_t  is_anomaly;       /* 1 if anomaly_score > threshold, else 0 */
} SensorFrame_t;
#pragma pack(pop)

#define SENSOR_FRAME_SIZE sizeof(SensorFrame_t)

/* Max stuffed frame size: worst case every payload+crc byte needs
 * escaping (2x) + 2 flag bytes. Gives generous headroom. */
#define HDLC_TX_BUF_SIZE ((SENSOR_FRAME_SIZE + 2) * 2 + 4)

uint16_t crc16_ccitt(const uint8_t *data, size_t len);

/* Encodes 'frame' into 'out' with HDLC framing + byte stuffing.
 * Returns number of bytes written to 'out', or 0 on overflow. */
uint16_t hdlc_encode_frame(const SensorFrame_t *frame, uint8_t *out, uint16_t out_size);

/* ============================================================
 * OTA additions -- generic (non-SensorFrame_t) encode/decode,
 * used for OTA command frames in both directions (ESP32->STM32
 * BEGIN/DATA/END, STM32->ESP32 ACK/NAK). Same FLAG/ESC framing
 * and CRC16-CCITT as above, just over an arbitrary byte payload
 * instead of a fixed SensorFrame_t struct.
 * ============================================================ */

/* Largest OTA payload we need to receive: 1 (cmd) + 4 (offset) +
 * 2 (len) + up to 512 (data chunk) = 519 bytes, matching the
 * ESP32 side's CHUNK_SIZE. Sized with margin. */
#define HDLC_RX_BUF_SIZE 560

#define OTA_ACK_BYTE 0xA5
#define OTA_NAK_BYTE 0x5A

typedef enum {
    HDLC_DECODE_IDLE = 0,
    HDLC_DECODE_IN_FRAME,
    HDLC_DECODE_ESCAPED,
} hdlc_decode_state_t;

typedef struct {
    hdlc_decode_state_t state;
    uint8_t  buf[HDLC_RX_BUF_SIZE];
    uint16_t idx;
} hdlc_decoder_t;

void hdlc_decoder_init(hdlc_decoder_t *dec);

/* Feed one raw received byte into the decoder.
 * Return value:
 *   > 0  -- a complete, CRC-verified frame is ready; return value is
 *           the payload length (CRC bytes already stripped), payload
 *           bytes are in dec->buf[0 .. return-1]
 *     0  -- byte consumed, frame not complete yet (normal case for
 *           most bytes)
 *    -1  -- frame ended but CRC check failed (frame discarded)
 *    -2  -- payload exceeded HDLC_RX_BUF_SIZE (frame discarded,
 *           decoder resyncs waiting for next FLAG)
 * Caller should treat any negative return as "no valid frame,
 * continue as normal" -- the decoder has already reset itself and
 * is ready for the next frame. */
int16_t hdlc_decoder_feed(hdlc_decoder_t *dec, uint8_t byte);

/* Same encoding as hdlc_encode_frame but for an arbitrary payload
 * buffer instead of a SensorFrame_t -- used for OTA ACK/NAK and any
 * other non-telemetry frame. */
uint16_t hdlc_encode_raw(const uint8_t *payload, uint16_t payload_len,
                          uint8_t *out, uint16_t out_size);

#endif /* SENSOR_PROTOCOL_H */
