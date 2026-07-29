#ifndef OTA_APPLY_H
#define OTA_APPLY_H

#include <stdbool.h>

/* Fetches manifest.json from manifest_url, picks the "slotA" or
 * "slotB" sub-object matching target_slot ("A" or "B"), downloads
 * that binary, verifies its SHA-256 against the manifest, and pushes
 * it to the STM32 over UART via stm32_ota_push().
 *
 * Blocks for the duration of the download + push (could be tens of
 * seconds) -- call this from a dedicated task, never directly from
 * the MQTT event callback (see main.c's mqtt_event_handler for where
 * this gets spawned).
 *
 * Returns true only if every step succeeded end-to-end: manifest
 * fetched and parsed, firmware downloaded, size and SHA-256 both
 * matched, and the STM32 accepted and confirmed the full transfer. */
bool ota_apply_from_manifest(const char *manifest_url, const char *target_slot);

#endif /* OTA_APPLY_H */
