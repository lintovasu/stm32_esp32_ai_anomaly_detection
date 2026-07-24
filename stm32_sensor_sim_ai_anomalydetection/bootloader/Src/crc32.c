#include "crc32.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
static uint32_t crc32_table[256];
static bool table_built = false;

static void build_table(void)
{
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++) {
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        }
        crc32_table[i] = c;
    }
    table_built = true;
}

uint32_t crc32_init(void)
{
    if (!table_built) build_table();
    return 0xFFFFFFFFu;
}

uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len)
{
    if (!table_built) build_table();
    for (size_t i = 0; i < len; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
    }
    return crc;
}

uint32_t crc32_finalize(uint32_t crc)
{
    return crc ^ 0xFFFFFFFFu;
}

uint32_t crc32_calc(const uint8_t *data, size_t len)
{
    uint32_t crc = crc32_init();
    crc = crc32_update(crc, data, len);
    return crc32_finalize(crc);
}
