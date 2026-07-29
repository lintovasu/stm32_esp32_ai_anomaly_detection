#ifndef OTA_CRC32_H
#define OTA_CRC32_H

#include <stdint.h>
#include <stddef.h>

/* Must produce bit-identical results to the STM32 bootloader's
 * bootloader/Src/crc32.c -- same algorithm (poly 0xEDB88320,
 * reflected, init 0xFFFFFFFF, final XOR 0xFFFFFFFF, the common
 * zlib-compatible CRC32). This is a SEPARATE check from crc16_ccitt
 * in sensor_protocol.c -- that one protects each individual UART
 * frame; this one verifies the whole firmware image end-to-end,
 * independent of how many frames it took to transfer. Implemented
 * here as a plain portable C version (not esp_rom_crc32_le) so there
 * is no ambiguity about matching bit-for-bit with the STM32 side. */
uint32_t ota_crc32(const uint8_t *data, size_t len);

#endif /* OTA_CRC32_H */
