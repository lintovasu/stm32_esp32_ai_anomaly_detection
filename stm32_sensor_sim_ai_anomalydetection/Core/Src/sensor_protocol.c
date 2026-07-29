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

/* ============================================================
 * OTA additions -- generic decoder + generic encoder.
 * ============================================================ */

uint16_t hdlc_encode_raw(const uint8_t *payload, uint16_t payload_len,
                          uint8_t *out, uint16_t out_size)
{
    uint16_t idx = 0;

    if (out_size < 4) return 0;
    out[idx++] = HDLC_FLAG;

    for (uint16_t i = 0; i < payload_len; i++) {
        idx = stuff_byte(payload[i], out, idx, out_size);
        if (idx == 0) return 0;
    }

    uint16_t crc = crc16_ccitt(payload, payload_len);
    idx = stuff_byte((crc >> 8) & 0xFF, out, idx, out_size);
    if (idx == 0) return 0;
    idx = stuff_byte(crc & 0xFF, out, idx, out_size);
    if (idx == 0) return 0;

    if (idx + 1 > out_size) return 0;
    out[idx++] = HDLC_FLAG;

    return idx;
}

void hdlc_decoder_init(hdlc_decoder_t *dec)
{
    dec->state = HDLC_DECODE_IDLE;
    dec->idx = 0;
}

int16_t hdlc_decoder_feed(hdlc_decoder_t *dec, uint8_t byte)
{
    if (byte == HDLC_FLAG) {
        if (dec->state == HDLC_DECODE_IN_FRAME && dec->idx >= 2) {
            /* Closing flag with at least 2 bytes buffered (the CRC) --
             * a complete frame. Split payload from the trailing CRC16
             * and verify. */
            uint16_t payload_len = dec->idx - 2;
            uint16_t rx_crc = ((uint16_t)dec->buf[payload_len] << 8)
                             | dec->buf[payload_len + 1];
            uint16_t calc_crc = crc16_ccitt(dec->buf, payload_len);

            dec->state = HDLC_DECODE_IDLE;
            dec->idx = 0;

            if (rx_crc == calc_crc) {
                return (int16_t)payload_len;
            }
            return -1; /* CRC mismatch */
        }
        /* Opening flag, or a stray/empty flag (e.g. back-to-back FLAGs,
         * or a flag seen while idle) -- (re)start frame capture. */
        dec->state = HDLC_DECODE_IN_FRAME;
        dec->idx = 0;
        return 0;
    }

    if (dec->state == HDLC_DECODE_IDLE) {
        /* Byte arrived outside any frame (line noise, or we're
         * resyncing after an error) -- ignore it. */
        return 0;
    }

    if (dec->state == HDLC_DECODE_ESCAPED) {
        byte ^= HDLC_ESC_XOR;
        dec->state = HDLC_DECODE_IN_FRAME;
    } else if (byte == HDLC_ESC) {
        dec->state = HDLC_DECODE_ESCAPED;
        return 0;
    }

    if (dec->idx >= HDLC_RX_BUF_SIZE) {
        /* Payload too large for our buffer -- discard and resync on
         * the next FLAG rather than overflowing. */
        dec->state = HDLC_DECODE_IDLE;
        dec->idx = 0;
        return -2;
    }

    dec->buf[dec->idx++] = byte;
    return 0;
}
