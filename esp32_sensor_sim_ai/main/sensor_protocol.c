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

hdlc_result_t hdlc_feed_byte(hdlc_decoder_t *dec, uint8_t byte, SensorFrame_t *out_frame)
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

        if (dec->idx != SENSOR_FRAME_SIZE + 2) {
            dec->idx = 0;
            return HDLC_FRAME_LENGTH_ERROR;
        }

        uint16_t rx_crc = ((uint16_t)dec->buf[SENSOR_FRAME_SIZE] << 8) |
                            dec->buf[SENSOR_FRAME_SIZE + 1];
        uint16_t calc_crc = crc16_ccitt(dec->buf, SENSOR_FRAME_SIZE);

        dec->idx = 0;

        if (rx_crc != calc_crc) {
            return HDLC_FRAME_CRC_ERROR;
        }

        memcpy(out_frame, dec->buf, SENSOR_FRAME_SIZE);
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
        /* Overflow - malformed frame, reset and wait for next FLAG */
        dec->in_frame = false;
        dec->idx = 0;
        return HDLC_FRAME_LENGTH_ERROR;
    }

    return HDLC_NEED_MORE_DATA;
}
