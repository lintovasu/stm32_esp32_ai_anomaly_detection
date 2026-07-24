#ifndef FLASH_LL_H
#define FLASH_LL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Thin wrapper around STM32 HAL flash functions so the rest of the code
 * doesn't need to deal with HAL_FLASH_Unlock/Lock or OB handles directly.
 * Link against STM32F4xx HAL (stm32f4xx_hal_flash.h / _flash_ex.h). */

/* Erases one sector by its hardware sector number (0-11 on F407VG). */
bool flash_ll_erase_sector(uint32_t sector_num);

/* Erases `count` consecutive sectors starting at `first_sector`. */
bool flash_ll_erase_sectors(uint32_t first_sector, uint32_t count);

/* Programs `len` bytes from `data` to `dest_addr`. dest_addr must be
 * within a previously-erased region. Programs in 32-bit words; len will
 * be rounded up internally (pad bytes should be 0xFF from the erase, or
 * the caller should pad `data` to a 4-byte boundary with 0xFF). */
bool flash_ll_write(uint32_t dest_addr, const uint8_t *data, size_t len);

#endif /* FLASH_LL_H */
