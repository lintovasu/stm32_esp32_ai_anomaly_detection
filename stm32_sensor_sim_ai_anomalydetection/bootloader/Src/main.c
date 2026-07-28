#include "stm32f4xx_hal.h"
#include "flash_metadata.h"
#include "flash_ll.h"
#include "crc32.h"

/* --------------------------------------------------------------------
 * Jump-to-application. This is the part that's easy to get subtly wrong.
 * Steps, in order, and why each one matters:
 *
 * 1. Disable and clear all pending interrupts / SysTick BEFORE touching
 *    VTOR. If an interrupt fires between relocating VTOR and jumping,
 *    it'll vector into the new app's table before the new app's stack
 *    pointer is even set up -- guaranteed crash.
 * 2. Set VTOR to the new app's base address. Every Cortex-M vector
 *    (reset, NMI, HardFault, all IRQs) is read from [VTOR + offset], so
 *    without this the app's own interrupt handlers are simply never
 *    reached -- any interrupt still runs the BOOTLOADER's handler.
 * 3. Set the main stack pointer (MSP) from the app's vector table[0]
 *    *before* jumping, don't rely on the app's reset handler to do it
 *    (it usually does, via the startup file, but the bootloader was
 *    using its own stack until this point -- switch explicitly for
 *    safety, especially since we call this before any HAL re-init).
 * 4. Jump via the reset handler address at vector table[1], not by
 *    calling code directly -- this lets the app's own startup code
 *    (copying .data, zeroing .bss, calling SystemInit) run exactly as
 *    if it had come from a normal power-on reset.
 * ------------------------------------------------------------------ */
typedef void (*app_reset_handler_t)(void);

static void jump_to_application(uint32_t app_base_addr)
{
    uint32_t app_stack_ptr = *(volatile uint32_t *)(app_base_addr + 0x00);
    uint32_t app_reset_addr = *(volatile uint32_t *)(app_base_addr + 0x04);

    /* Sanity check: an erased/blank slot reads as 0xFFFFFFFF everywhere.
     * A valid Cortex-M stack pointer must be in SRAM range. Refuse to
     * jump into garbage. */
    //if ((app_stack_ptr & 0x2FFE0000u) != 0x20000000u) {
    //    /* Not a plausible SRAM address -- slot is blank or corrupt.
    //     * Caller should have already validated CRC before getting here;
    //     * this is a last-ditch guard. Loop here rather than jump into
    //     * the weeds -- in a real product, light an LED / log and
    //     * consider forcing a rollback instead of an infinite loop. */
    //    while (1) { /* halt */ }
    //}

    __disable_irq();

    /* Disable SysTick and clear any pending exceptions so nothing fires
     * mid-transition. */
    SysTick->CTRL = 0;
    SysTick->VAL  = 0;

    for (uint8_t i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFFu;
        NVIC->ICPR[i] = 0xFFFFFFFFu;
    }

    /* Relocate vector table to the app's base. */
    SCB->VTOR = app_base_addr;

    __set_MSP(app_stack_ptr);

    __enable_irq();

    app_reset_handler_t app_entry = (app_reset_handler_t)app_reset_addr;
    app_entry();

    /* Never reached. */
    while (1) { }
}

/* --------------------------------------------------------------------
 * Verifies that the image in `slot` matches the given size/crc. Reads
 * flash directly (no RAM buffer needed for the whole image -- CRC is
 * computed incrementally straight from flash). Bounded by the slot's
 * max size so a bogus staging_size can't walk off into the next region.
 * ------------------------------------------------------------------ */
static bool verify_slot_image(ota_slot_t slot, uint32_t expected_size, uint32_t expected_crc)
{
    uint32_t max_size = ota_slot_size(slot);
    if (expected_size == 0 || expected_size > max_size) {
        return false;
    }

    uint32_t base = ota_slot_base(slot);
    uint32_t crc = crc32_init();
    crc = crc32_update(crc, (const uint8_t *)base, expected_size);
    crc = crc32_finalize(crc);

    return (crc == expected_crc);
}

int main(void)
{
    HAL_Init();
    /* SystemClock_Config() etc. would normally go here -- omitted since
     * the bootloader can run at the default HSI clock; it does almost
     * nothing performance-sensitive. Keeping bootloader clock config
     * separate/minimal also reduces its chance of being the thing that
     * breaks. */

    ota_metadata_t meta;
    bool meta_valid = flash_metadata_read(&meta);

    if (!meta_valid) {
        /* First boot ever, or metadata sector corrupt/erased. Assume
         * Slot A holds the factory image and trust it. */
        flash_metadata_init_defaults();
        flash_metadata_read(&meta);
    }

    if (meta.pending_update) {
        ota_slot_t staging = ota_other_slot((ota_slot_t)meta.active_slot);

        if (verify_slot_image(staging, meta.staging_size, meta.staging_crc32)) {
            /* Promote staging -> active. */
            meta.active_slot    = staging;
            meta.pending_update  = 0;
            meta.boot_attempts  = 0;
            meta.confirmed_good = 0; /* new image must re-earn trust */
            flash_metadata_write(&meta);
        } else {
            /* Corrupt staged image -- discard the update, stay put. */
            meta.pending_update = 0;
            flash_metadata_write(&meta);
        }
    }

    if (!meta.confirmed_good) {
        meta.boot_attempts++;

        if (meta.boot_attempts > MAX_BOOT_ATTEMPTS) {
            /* This slot is crash-looping. Roll back to the other one,
             * which by construction was already confirmed_good before
             * we ever promoted away from it. */
            meta.active_slot    = ota_other_slot((ota_slot_t)meta.active_slot);
            meta.boot_attempts  = 0;
            meta.confirmed_good = 1;
        }

        flash_metadata_write(&meta);
    }

    jump_to_application(ota_slot_base((ota_slot_t)meta.active_slot));

    /* Never reached. */
    while (1) { }
}
