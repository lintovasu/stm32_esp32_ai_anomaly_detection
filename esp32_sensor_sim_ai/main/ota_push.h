#ifndef OTA_PUSH_H
#define OTA_PUSH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Call once from app_main(), after uart_init() -- creates the queue
 * that uart_rx_task uses to hand OTA ACK/NAK bytes to whichever push
 * operation is currently waiting for one. */
void ota_push_init(void);

/* Called from uart_rx_task (see main.c) whenever a decoded frame's
 * payload is exactly 1 byte -- the only frame shape used for OTA
 * ACK/NAK responses, distinct from the SENSOR_FRAME_SIZE telemetry
 * frames on this same link. */
void ota_push_on_ack_byte(uint8_t byte);

/* Pushes 'total_size' bytes from 'image' to the STM32 over UART using
 * the BEGIN/DATA/END protocol (see stm32_ota/esp32_host/ota_uart_protocol.md).
 * 'image' must already be downloaded and verified (SHA-256 checked
 * against the manifest) before calling this -- this function only
 * handles the UART handshake + per-chunk retry, not the download.
 * Blocks until the whole transfer succeeds or fails; safe to call
 * from a dedicated task, not from the MQTT event callback directly. */
bool stm32_ota_push(const uint8_t *image, uint32_t total_size);

#endif /* OTA_PUSH_H */
