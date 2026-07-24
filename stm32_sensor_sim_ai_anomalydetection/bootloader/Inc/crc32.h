#ifndef CRC32_H
#define CRC32_H

#include <stdint.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
/* Standard CRC32 (poly 0xEDB88320, reflected, init 0xFFFFFFFF, final XOR
 * 0xFFFFFFFF) -- i.e. the common "CRC32" used by zlib/Ethernet/etc. Use
 * the same implementation on the ESP32 side (zlib's crc32() matches this
 * exactly) so the value the ESP32 sends and the value the STM32
 * recomputes are directly comparable. NOT the same as the CRC16-CCITT
 * used in your HDLC frame checksums -- this is a separate, larger check
 * over the whole firmware image. */
uint32_t crc32_calc(const uint8_t *data, size_t len);

/* Incremental variant for streaming data as it arrives chunk by chunk.
 * Call crc32_init(), then crc32_update() per chunk, then
 * crc32_finalize() once at the end. */
uint32_t crc32_init(void);
uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len);
uint32_t crc32_finalize(uint32_t crc);

#endif /* CRC32_H */
