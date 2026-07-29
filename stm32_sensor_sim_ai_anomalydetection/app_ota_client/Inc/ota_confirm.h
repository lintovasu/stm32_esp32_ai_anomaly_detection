#ifndef OTA_CONFIRM_H
#define OTA_CONFIRM_H

#include <stdbool.h>

/* Declarations for the two functions defined in ota_confirm.c.
 * Call ota_confirm_good() a few seconds after boot, once the app has
 * verified it's actually healthy -- see main.c's main loop for where
 * this is called and ota_confirm.c for what it does to the metadata
 * sector. */
bool ota_confirm_good(void);

/* Forces a clean reset so control passes to the bootloader, which will
 * then apply a staged update if one is pending. Never returns. */
void ota_reboot_to_apply_update(void);

#endif /* OTA_CONFIRM_H */
