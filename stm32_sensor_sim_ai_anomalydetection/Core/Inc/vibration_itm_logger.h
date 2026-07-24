#ifndef VIBRATION_ITM_LOGGER_H
#define VIBRATION_ITM_LOGGER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Call once at startup, before the main loop, to enable ITM stimulus port 0. */
void itm_init(void);

/* Non-blocking-ish: checks elapsed time internally and sends one
 * vibration_rms_g reading over ITM whenever SAMPLE_PERIOD_MS has
 * elapsed. Call this repeatedly from your main while(1) loop. */
void vibration_logging_task(void);

#ifdef __cplusplus
}
#endif

#endif /* VIBRATION_ITM_LOGGER_H */
