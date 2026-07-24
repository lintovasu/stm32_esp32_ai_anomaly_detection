#include "ota_receiver.h"
#include "flash_metadata.h"
#include "flash_ll.h"
#include "crc32.h"

static ota_slot_t staging_slot;
static uint32_t   staging_base;
static uint32_t   staging_capacity;
static uint32_t   expected_total_size;
static uint32_t   bytes_written;
static bool       transfer_active;
static uint32_t   running_crc;

void ota_receiver_init(void)
{
    ota_metadata_t meta;
    bool valid = flash_metadata_read(&meta);

    /* If metadata is somehow invalid at this point, default to treating
     * Slot B as staging -- matches flash_metadata_init_defaults()'s
     * choice of Slot A as active. */
    ota_slot_t active = valid ? (ota_slot_t)meta.active_slot : SLOT_A;
    staging_slot     = ota_other_slot(active);
    staging_base     = ota_slot_base(staging_slot);
    staging_capacity = ota_slot_size(staging_slot);
    transfer_active  = false;
}

ota_result_t ota_receiver_begin(uint32_t total_size)
{
    if (transfer_active) {
        return OTA_ERR_ALREADY_STARTED;
    }
    if (total_size == 0 || total_size > staging_capacity) {
        return OTA_ERR_TOO_LARGE;
    }

    /* Erase the whole staging slot up front. This blocks for roughly
     * 1 second per 128KB sector on F407 -- do this call from a
     * dedicated low-priority FreeRTOS task, NOT from the UART RX ISR
     * or a high-priority task, or you'll starve everything else
     * (sensor sampling, CAN handling) for several seconds. */
    ota_slot_t staging = staging_slot;
    uint32_t first_sector = (staging == SLOT_A) ? SLOT_A_FIRST_SECTOR : SLOT_B_FIRST_SECTOR;
    uint32_t num_sectors  = (staging == SLOT_A) ? SLOT_A_NUM_SECTORS  : SLOT_B_NUM_SECTORS;

    if (!flash_ll_erase_sectors(first_sector, num_sectors)) {
        return OTA_ERR_ERASE_FAILED;
    }

    expected_total_size = total_size;
    bytes_written        = 0;
    running_crc          = crc32_init();
    transfer_active       = true;

    return OTA_OK;
}

ota_result_t ota_receiver_data(uint32_t offset, const uint8_t *data, uint16_t len)
{
    if (!transfer_active) {
        return OTA_ERR_NOT_STARTED;
    }

    /* This simple version requires strictly in-order, contiguous
     * chunks. To support resume-after-disconnect, you'd instead: track
     * a bitmap of which chunks have arrived, allow offset to be
     * non-sequential, and only compute the running CRC once the whole
     * image is present (read back from flash) rather than
     * incrementally here. Left as a documented TODO since your UART
     * link is a short, low-loss hop, so simple in-order transfer with a
     * retry-the-whole-thing-on-failure policy is a reasonable v1. */
    if (offset != bytes_written) {
        return OTA_ERR_BAD_OFFSET;
    }
    if (offset + len > expected_total_size) {
        return OTA_ERR_TOO_LARGE;
    }

    if (!flash_ll_write(staging_base + offset, data, len)) {
        return OTA_ERR_WRITE_FAILED;
    }

    running_crc    = crc32_update(running_crc, data, len);
    bytes_written += len;

    return OTA_OK;
}

ota_result_t ota_receiver_end(uint32_t expected_crc32)
{
    if (!transfer_active) {
        return OTA_ERR_NOT_STARTED;
    }
    if (bytes_written != expected_total_size) {
        transfer_active = false;
        return OTA_ERR_BAD_OFFSET;
    }

    uint32_t final_crc = crc32_finalize(running_crc);
    if (final_crc != expected_crc32) {
        transfer_active = false;
        return OTA_ERR_CRC_MISMATCH;
    }

    ota_metadata_t meta;
    flash_metadata_read(&meta); /* tolerate invalid magic here; overwritten below */

    meta.magic          = OTA_META_MAGIC;
    meta.pending_update  = 1;
    meta.staging_size    = expected_total_size;
    meta.staging_crc32   = final_crc;
    /* Leave active_slot, boot_attempts, confirmed_good untouched here --
     * the bootloader owns those transitions on next reset. */

    transfer_active = false;

    if (!flash_metadata_write(&meta)) {
        return OTA_ERR_METADATA_WRITE_FAILED;
    }

    return OTA_OK;
    /* Caller (your main app / MQTT-diag task) should now ACK success to
     * the ESP32-S3 over UART, then call NVIC_SystemReset() (or your own
     * clean-shutdown-then-reset routine) to hand off to the bootloader. */
}
