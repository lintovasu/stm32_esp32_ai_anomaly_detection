#ifndef AI_INFERENCE_H
#define AI_INFERENCE_H

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * PASTE THESE VALUES FROM THE TRAINING SCRIPT OUTPUT
 * (ai_training/train_anomaly_model.py console output)
 * ============================================================ */
#define WINDOW_SIZE  10
#define N_CHANNELS   3
#define INPUT_DIM    (WINDOW_SIZE * N_CHANNELS)   /* 30 */

/* REPLACE with the printed threshold from training */
#define ANOMALY_THRESHOLD  0.107469f   /* PLACEHOLDER - overwrite this */

/* REPLACE with the printed feature_mean[] / feature_std[] arrays
 * from training. Left as flat zero/one arrays here so the project
 * compiles out of the box before you've trained a real model. */
static const float feature_mean[INPUT_DIM] = {
    45.027378f,
    0.800063f,
    101.301414f,
    45.027245f,
    0.800070f,
    101.301392f,
    45.027153f,
    0.800082f,
    101.301376f,
    45.027092f,
    0.800091f,
    101.301361f,
    45.027050f,
    0.800097f,
    101.301353f,
    45.026882f,
    0.800100f,
    101.301338f,
    45.026730f,
    0.800098f,
    101.301308f,
    45.026611f,
    0.800096f,
    101.301285f,
    45.026466f,
    0.800088f,
    101.301262f,
    45.026310f,
    0.800086f,
    101.301231f
};

static const float feature_std[INPUT_DIM] = {
    3.632270f,
    0.219779f,
    0.372169f,
    3.632304f,
    0.219785f,
    0.372184f,
    3.632343f,
    0.219794f,
    0.372193f,
    3.632368f,
    0.219803f,
    0.372198f,
    3.632389f,
    0.219808f,
    0.372209f,
    3.632400f,
    0.219811f,
    0.372231f,
    3.632387f,
    0.219809f,
    0.372251f,
    3.632393f,
    0.219808f,
    0.372262f,
    3.632394f,
    0.219799f,
    0.372283f,
    3.632419f,
    0.219797f,
    0.372309f
};

/* Call once at startup, after HAL_Init(). Initializes the AI runtime. */
bool ai_inference_init(void);

/* Push one new (temperature, vibration, pressure) sample into the
 * sliding window. Returns true once a full window is available and
 * inference has been run - in which case *out_score and *out_is_anomaly
 * are populated. Returns false while still filling the initial window. */
bool ai_inference_update(float temperature_c, float vibration_rms_g,
                          float pressure_kpa, float *out_score, uint8_t *out_is_anomaly);

#endif /* AI_INFERENCE_H */
