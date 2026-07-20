#ifndef VIBRATION_RMS_H
#define VIBRATION_RMS_H

#include <stdint.h>

/* Number of raw accel samples averaged into one RMS value per call.
 * Higher = smoother/more accurate RMS but slower to produce a reading.
 * Tune this to your MPU6050 output data rate and how fast you need
 * a fresh vibration_rms_g value for the 10-sample sliding window. */
#define VIBRATION_RMS_SAMPLE_COUNT  32

/* Reads VIBRATION_RMS_SAMPLE_COUNT samples from the MPU6050 and returns
 * the RMS of the vibration magnitude (gravity removed), in g's.
 * Call this once per "vibration_rms_g" value you feed into
 * ai_inference_update(). Blocking - takes SAMPLE_COUNT * sample_interval. */
float vibration_rms_read(void);

#endif /* VIBRATION_RMS_H */
