#include "sensor_protocol.h"

uint16_t crc16_ccitt(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

static uint16_t stuff_byte(uint8_t b, uint8_t *out, uint16_t idx, uint16_t out_size)
{
    if (b == HDLC_FLAG || b == HDLC_ESC) {
        if (idx + 2 > out_size) return 0;
        out[idx++] = HDLC_ESC;
        out[idx++] = b ^ HDLC_ESC_XOR;
    } else {
        if (idx + 1 > out_size) return 0;
        out[idx++] = b;
    }
    return idx;
}

uint16_t hdlc_encode_frame(const SensorFrame_t *frame, uint8_t *out, uint16_t out_size)
{
    uint16_t idx = 0;
    const uint8_t *payload = (const uint8_t *)frame;

    if (out_size < 4) return 0;
    out[idx++] = HDLC_FLAG;

    for (size_t i = 0; i < SENSOR_FRAME_SIZE; i++) {
        idx = stuff_byte(payload[i], out, idx, out_size);
        if (idx == 0) return 0;
    }

    uint16_t crc = crc16_ccitt(payload, SENSOR_FRAME_SIZE);
    idx = stuff_byte((crc >> 8) & 0xFF, out, idx, out_size);
    if (idx == 0) return 0;
    idx = stuff_byte(crc & 0xFF, out, idx, out_size);
    if (idx == 0) return 0;

    if (idx + 1 > out_size) return 0;
    out[idx++] = HDLC_FLAG;

    return idx;
}
