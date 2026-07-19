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

/* Decoder holds accumulated de-stuffed payload+crc bytes for one frame */
typedef struct {
    uint8_t  buf[SENSOR_FRAME_SIZE + 2]; /* payload + 2 CRC bytes */
    uint16_t idx;
    bool     in_frame;
    bool     esc_pending;
} hdlc_decoder_t;

typedef enum {
    HDLC_NEED_MORE_DATA,
    HDLC_FRAME_COMPLETE,
    HDLC_FRAME_CRC_ERROR,
    HDLC_FRAME_LENGTH_ERROR,
} hdlc_result_t;

uint16_t crc16_ccitt(const uint8_t *data, size_t len);

/* Feed one raw byte from UART into the decoder. On HDLC_FRAME_COMPLETE,
 * 'out_frame' is populated with the decoded, CRC-verified sensor data. */
hdlc_result_t hdlc_feed_byte(hdlc_decoder_t *dec, uint8_t byte, SensorFrame_t *out_frame);

#endif /* SENSOR_PROTOCOL_H */
