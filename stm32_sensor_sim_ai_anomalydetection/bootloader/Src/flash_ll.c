#include "flash_ll.h"
#include "stm32f4xx_hal.h"

bool flash_ll_erase_sector(uint32_t sector_num)
{
    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef erase = {0};
    uint32_t sector_error = 0;

    erase.TypeErase    = FLASH_TYPEERASE_SECTORS;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3; /* assumes VDD 2.7-3.6V, adjust if different */
    erase.Sector       = sector_num;
    erase.NbSectors    = 1;

    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&erase, &sector_error);

    HAL_FLASH_Lock();

    return (status == HAL_OK) && (sector_error == 0xFFFFFFFFu);
}

bool flash_ll_erase_sectors(uint32_t first_sector, uint32_t count)
{
    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef erase = {0};
    uint32_t sector_error = 0;

    erase.TypeErase    = FLASH_TYPEERASE_SECTORS;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    erase.Sector       = first_sector;
    erase.NbSectors    = count;

    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&erase, &sector_error);

    HAL_FLASH_Lock();

    return (status == HAL_OK) && (sector_error == 0xFFFFFFFFu);
}

bool flash_ll_write(uint32_t dest_addr, const uint8_t *data, size_t len)
{
    HAL_FLASH_Unlock();

    bool ok = true;
    size_t i = 0;

    /* Program in 32-bit words. If len isn't a multiple of 4, the last
     * partial word is padded with 0xFF (erased-flash value) so we never
     * read/write past the caller's buffer. */
    while (i < len) {
        uint32_t word = 0xFFFFFFFFu;
        size_t remaining = len - i;
        size_t chunk = (remaining >= 4) ? 4 : remaining;
        for (size_t b = 0; b < chunk; b++) {
            word &= ~(0xFFu << (8 * b));
            word |= ((uint32_t)data[i + b]) << (8 * b);
        }

        HAL_StatusTypeDef status = HAL_FLASH_Program(
            FLASH_TYPEPROGRAM_WORD, dest_addr + i, word);

        if (status != HAL_OK) {
            ok = false;
            break;
        }

        /* Verify readback -- cheap insurance against a silently failed
         * program operation (e.g. brown-out mid-write). */
        if (*(volatile uint32_t *)(dest_addr + i) != word) {
            ok = false;
            break;
        }

        i += 4;
    }

    HAL_FLASH_Lock();
    return ok;
}
