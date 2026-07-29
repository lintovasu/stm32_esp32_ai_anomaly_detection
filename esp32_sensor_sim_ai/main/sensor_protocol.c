#include "sensor_protocol.h"
#include <string.h>

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

hdlc_result_t hdlc_feed_byte(hdlc_decoder_t *dec, uint8_t byte,
                              uint8_t *out_buf, uint16_t out_buf_size,
                              uint16_t *out_len)
{
    if (byte == HDLC_FLAG) {
        if (!dec->in_frame) {
            /* Start of a new frame */
            dec->in_frame = true;
            dec->idx = 0;
            dec->esc_pending = false;
            return HDLC_NEED_MORE_DATA;
        }

        /* Second FLAG = end of frame (ignore empty/false frames from
         * back-to-back flag bytes) */
        dec->in_frame = false;

        if (dec->idx == 0) {
            return HDLC_NEED_MORE_DATA; /* stray flag, not a real frame */
        }

        /* Need at least the 2 trailing CRC bytes to have a real payload. */
        if (dec->idx < 2) {
            dec->idx = 0;
            return HDLC_FRAME_LENGTH_ERROR;
        }

        uint16_t payload_len = dec->idx - 2;
        uint16_t rx_crc = ((uint16_t)dec->buf[payload_len] << 8) |
                            dec->buf[payload_len + 1];
        uint16_t calc_crc = crc16_ccitt(dec->buf, payload_len);

        dec->idx = 0;

        if (rx_crc != calc_crc) {
            return HDLC_FRAME_CRC_ERROR;
        }

        if (payload_len > out_buf_size) {
            /* Caller's buffer can't hold it -- shouldn't happen given
             * out_buf_size is normally sizeof(dec->buf), but guard
             * anyway rather than overrun the caller's buffer. */
            return HDLC_FRAME_LENGTH_ERROR;
        }

        memcpy(out_buf, dec->buf, payload_len);
        *out_len = payload_len;
        return HDLC_FRAME_COMPLETE;
    }

    if (!dec->in_frame) {
        return HDLC_NEED_MORE_DATA; /* junk outside a frame, ignore */
    }

    if (byte == HDLC_ESC) {
        dec->esc_pending = true;
        return HDLC_NEED_MORE_DATA;
    }

    uint8_t actual = byte;
    if (dec->esc_pending) {
        actual = byte ^ HDLC_ESC_XOR;
        dec->esc_pending = false;
    }

    if (dec->idx < sizeof(dec->buf)) {
        dec->buf[dec->idx++] = actual;
    } else {
        /* Overflow - malformed/oversized frame, reset and wait for
         * next FLAG rather than corrupting adjacent memory. */
        dec->in_frame = false;
        dec->idx = 0;
        return HDLC_FRAME_LENGTH_ERROR;
    }

    return HDLC_NEED_MORE_DATA;
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
