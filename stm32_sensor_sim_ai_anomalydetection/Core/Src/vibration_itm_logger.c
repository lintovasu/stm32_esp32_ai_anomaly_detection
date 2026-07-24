/*
 * vibration_itm_logger.c
 * ======================================================
 * Reads MPU6050 accelerometer samples, computes an RMS vibration
 * value, and streams it out over ITM/SWO (stimulus port 0) as
 * plain text, one float per line -- ready to be captured on the
 * PC side and turned into vibration_log.csv.
 *
 * Assumes:
 *   - MPU6050 I2C driver already returns accel in g via
 *     MPU6050_Read_Accel_g(&ax, &ay, &az) (as in your iot-firmware
 *     project's mpu6050.h/.c).
 *   - CubeMX Debug config set to "Trace Asynchronous Sw" so PB3
 *     is routed as SYS_SWO.
 *   - HAL_Delay/SysTick or an RTOS timer available for pacing.
 * ======================================================
 */

#include "stm32f4xx_hal.h"
#include "vibration_itm_logger.h"
#include "mpu6050.h"
#include <stdio.h>
#include <math.h>

#define SAMPLE_PERIOD_MS      500     // must match SAMPLE_PERIOD_S in training script
#define ACCEL_SUBSAMPLES      50      // raw accel reads averaged into one RMS value

/* ---- ITM setup: call once at startup, before the main loop ---- */
void itm_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    ITM->LAR = 0xC5ACCE55;      // unlock ITM registers
    ITM->TER |= (1UL << 0);      // enable stimulus port 0
    ITM->TCR |= ITM_TCR_ITMENA_Msk;
}

/* ---- retarget printf to ITM stimulus port 0 ---- */
int _write(int file, char *ptr, int len)
{
    for (int i = 0; i < len; i++) {
        ITM_SendChar((uint32_t)ptr[i]);
    }
    return len;
}

/* ---- one RMS vibration reading, in g ---- */
static float read_vibration_rms_g(void)
{
    float sum_sq = 0.0f;
    float ax, ay, az;

    for (int i = 0; i < ACCEL_SUBSAMPLES; i++) {
        MPU6050_Read_Accel_g(&ax, &ay, &az);   // driver already returns g directly

        /* subtract 1g so a perfectly still, level sensor reads ~0,
           not ~1 (gravity on the Z axis) -- adjust if your mounting
           orientation differs */
        float mag = sqrtf(ax * ax + ay * ay + az * az) - 1.0f;

        sum_sq += mag * mag;
        HAL_Delay(1);   // small spacing between subsamples
    }

    return sqrtf(sum_sq / ACCEL_SUBSAMPLES);
}

/* ---- call this repeatedly from your existing main while(1) loop ----
 * Non-blocking: does nothing until SAMPLE_PERIOD_MS has elapsed since
 * the last reading, then sends one value and returns. Call itm_init()
 * once before the loop starts, NOT from in here. */
void vibration_logging_task(void)
{
    static uint32_t last_tick = 0;

    uint32_t now = HAL_GetTick();
    if ((now - last_tick) >= SAMPLE_PERIOD_MS) {
        last_tick = now;

        float vibration_rms_g = read_vibration_rms_g();

        /* one value per line, matches load_real_vibration_data() */
        printf("%.6f\r\n", vibration_rms_g);
    }
}
