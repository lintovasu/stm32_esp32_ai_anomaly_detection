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

#endif /* SENSOR_PROTOCOL_H */
