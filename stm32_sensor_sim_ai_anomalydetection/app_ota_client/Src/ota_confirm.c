#include "flash_metadata.h"
#include "flash_ll.h"
#include "stm32f4xx_hal.h"  /* pulls in CMSIS core_cm4.h, which declares NVIC_SystemReset() */
#include <stdbool.h>

/* Call this a few seconds after boot, once the app has verified it's
 * actually healthy -- e.g. MPU6050 responds on I2C, FreeRTOS tasks are
 * all alive, ESP32-S3 UART link is up, first MQTT publish succeeded.
 * Until this is called, boot_attempts keeps climbing on every reset and
 * a crash-loop will trigger automatic rollback in the bootloader. */
bool ota_confirm_good(void)
{
    ota_metadata_t meta;
    if (!flash_metadata_read(&meta)) {
        return false; /* shouldn't happen once past the bootloader */
    }

    if (meta.confirmed_good) {
        return true; /* already confirmed, nothing to do */
    }

    meta.confirmed_good = 1;
    meta.boot_attempts   = 0;
    return flash_metadata_write(&meta);
}

/* Optional: lets the app itself force a reboot into "check for update"
 * behavior, e.g. after ota_receiver_end() succeeds, or on an operator
 * command over MQTT. The bootloader doesn't need a special mode for
 * this -- it always checks pending_update on every reset -- so this is
 * just a plain reset. Kept as a named function for clarity at call
 * sites and so you have one place to add pre-reset cleanup later
 * (flushing telemetry, de-asserting CAN, etc). */
void ota_reboot_to_apply_update(void)
{
    /* TODO: flush/quiesce peripherals here before resetting if needed */
    NVIC_SystemReset();
}
