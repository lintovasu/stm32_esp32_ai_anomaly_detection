/* ============================================================
 * vibration_rms.c
 * ============================================================
 * Computes real vibration RMS (in g) from the MPU6050, to replace
 * a simulated/placeholder value in your anomaly detection pipeline.
 *
 * ASSUMPTION - adjust to match your actual driver:
 *   This calls MPU6050_Read_Accel_g(float *ax, float *ay, float *az),
 *   assumed to return calibrated acceleration in g's for each axis.
 *   If your existing driver has a different function name/signature
 *   (e.g. returns raw int16 LSB counts instead of g's), swap the call
 *   in vibration_rms_read() below and adjust the scaling accordingly.
 * ============================================================ */

#include "vibration_rms.h"
#include <math.h>

/* --- Replace with your actual driver include --- */
#include "mpu6050.h"   /* wherever your existing I2C driver's header lives */

float vibration_rms_read(void)
{
    float sum_sq = 0.0f;

    for (uint16_t i = 0; i < VIBRATION_RMS_SAMPLE_COUNT; i++) {
        float ax, ay, az;

        /* --- Adjust this call to match your existing MPU6050 driver --- */
        MPU6050_Read_Accel_g(&ax, &ay, &az);

        /* Total acceleration magnitude */
        float mag = sqrtf(ax * ax + ay * ay + az * az);

        /* Remove gravity's ~1g baseline so a stationary sensor reads
         * near 0 vibration instead of ~1g constant offset. If your
         * sensor orientation means gravity isn't purely on one axis,
         * subtracting the magnitude's resting value (~1.0g) like this
         * is the simplest correct approach regardless of mounting angle. */
        float vibration = mag - 1.0f;

        sum_sq += vibration * vibration;

        /* If your driver doesn't block/pace itself, add a small delay
         * here matching your desired sample rate, e.g. HAL_Delay(1)
         * for ~1kHz, or use a timer/DMA-driven capture instead of
         * polling in a tight loop. */
    }

    float rms = sqrtf(sum_sq / (float)VIBRATION_RMS_SAMPLE_COUNT);
    return rms;
}
