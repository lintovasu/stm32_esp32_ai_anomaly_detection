#ifndef FLASH_METADATA_H
#define FLASH_METADATA_H

#include <stdint.h>
#include <stdbool.h>

/* This header is shared by the bootloader AND the application's OTA
 * receiver module -- both need to read/write the same struct layout.
 * Keep it in sync in both places (or better: symlink / share the file
 * in your real build). */

#define OTA_META_MAGIC        0x4F544131u   /* "OTA1" */

#define FLASH_BASE_ADDR       0x08000000u

#define BOOTLOADER_BASE       (FLASH_BASE_ADDR + 0x00000000u) /* sectors 0-1 */
#define METADATA_SECTOR_ADDR  (FLASH_BASE_ADDR + 0x00008000u) /* sector 2 */
#define METADATA_SECTOR_NUM   2u
#define RESERVED_SECTOR_ADDR  (FLASH_BASE_ADDR + 0x0000C000u) /* sector 3, spare */

#define SLOT_A_BASE           (FLASH_BASE_ADDR + 0x00010000u) /* sectors 4-7  */
#define SLOT_A_SIZE           (448u * 1024u)
#define SLOT_A_FIRST_SECTOR   4u
#define SLOT_A_NUM_SECTORS    4u

#define SLOT_B_BASE           (FLASH_BASE_ADDR + 0x00080000u) /* sectors 8-11 */
#define SLOT_B_SIZE           (512u * 1024u)
#define SLOT_B_FIRST_SECTOR   8u
#define SLOT_B_NUM_SECTORS    4u

#define MAX_BOOT_ATTEMPTS     3u

typedef enum {
    SLOT_A = 0,
    SLOT_B = 1
} ota_slot_t;

/* Fits well within one flash word-aligned block; padded to 32 bytes so
 * future fields can be added without breaking alignment. */
typedef struct {
    uint32_t magic;            /* OTA_META_MAGIC if the sector is valid   */
    uint32_t active_slot;      /* ota_slot_t: which slot to boot          */
    uint32_t pending_update;   /* 1 = staging slot holds a new image      */
    uint32_t staging_size;     /* size in bytes of the staged image       */
    uint32_t staging_crc32;    /* CRC32 the ESP32 computed over the image */
    uint32_t boot_attempts;    /* consecutive un-confirmed boots          */
    uint32_t confirmed_good;   /* 1 = active app has passed self-check    */
    uint32_t reserved;         /* padding / future use                    */
} ota_metadata_t;

/* Returns the base address of a given slot. */
static inline uint32_t ota_slot_base(ota_slot_t slot) {
    return (slot == SLOT_A) ? SLOT_A_BASE : SLOT_B_BASE;
}

static inline uint32_t ota_slot_size(ota_slot_t slot) {
    return (slot == SLOT_A) ? SLOT_A_SIZE : SLOT_B_SIZE;
}

static inline ota_slot_t ota_other_slot(ota_slot_t slot) {
    return (slot == SLOT_A) ? SLOT_B : SLOT_A;
}

/* Reads the metadata sector into *out. Returns false if magic is invalid
 * (caller should treat this as "first boot ever" and call
 * flash_metadata_init()). */
bool flash_metadata_read(ota_metadata_t *out);

/* Erases the metadata sector and writes a fresh struct. Used both for
 * first-time init and for every update to the struct (since sector 2 is
 * only 16 KB and this is not a high-frequency write, erase+rewrite is
 * simple and adequate -- no wear-leveling needed at this update rate). */
bool flash_metadata_write(const ota_metadata_t *meta);

/* Convenience: writes an all-defaults struct (active_slot = SLOT_A,
 * everything else zeroed, confirmed_good = 1 since Slot A is presumed to
 * be the factory image). */
bool flash_metadata_init_defaults(void);

#endif /* FLASH_METADATA_H */
