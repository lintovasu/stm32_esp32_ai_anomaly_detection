#include "flash_metadata.h"
#include "flash_ll.h"
#include <string.h>

bool flash_metadata_read(ota_metadata_t *out)
{
    memcpy(out, (const void *)METADATA_SECTOR_ADDR, sizeof(ota_metadata_t));
    return (out->magic == OTA_META_MAGIC);
}

bool flash_metadata_write(const ota_metadata_t *meta)
{
    if (!flash_ll_erase_sector(METADATA_SECTOR_NUM)) {
        return false;
    }
    return flash_ll_write(METADATA_SECTOR_ADDR,
                           (const uint8_t *)meta,
                           sizeof(ota_metadata_t));
}

bool flash_metadata_init_defaults(void)
{
    ota_metadata_t meta = {0};
    meta.magic          = OTA_META_MAGIC;
    meta.active_slot     = SLOT_A;
    meta.pending_update  = 0;
    meta.staging_size    = 0;
    meta.staging_crc32   = 0;
    meta.boot_attempts   = 0;
    meta.confirmed_good  = 1; /* factory image is trusted by definition */
    meta.reserved        = 0;

    return flash_metadata_write(&meta);
}
