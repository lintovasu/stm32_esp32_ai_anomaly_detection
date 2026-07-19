/* ============================================================
 * STM32 Simulated Sensor Node -> ESP32-S3 -> MQTT
 * ============================================================
 * Generates plausible temperature / vibration / pressure values
 * (sine wave + pseudo-random jitter, no real sensor attached),
 * packs them into a struct, wraps in an HDLC frame with CRC16,
 * and sends over UART every 500ms.
 *
 * CubeMX configuration required:
 *   - USART2, Asynchronous, 115200 8N1 (same as hello world project)
 *   - No FreeRTOS required for this version - bare main loop
 *
 * Wiring (same as before):
 *   STM32 TX (PA2) -> ESP32-S3 RX (GPIO17)
 *   STM32 RX (PA3) -> ESP32-S3 TX (GPIO18)
 *   STM32 GND      -> ESP32-S3 GND
 * ============================================================ */

#include "main.h"
#include "sensor_protocol.h"
#include "ai_inference.h"
#include <math.h>
#include <stdlib.h>

UART_HandleTypeDef huart2;

static void SystemClock_Config(void);
static void MX_USART2_UART_Init(void);

#define SENSOR_ID       0x01
#define SAMPLE_PERIOD_MS 500

/* Simple deterministic-ish pseudo-random jitter without needing
 * a hardware RNG peripheral - good enough for simulation purposes. */
static float pseudo_noise(float amplitude)
{
    static uint32_t seed = 12345;
    seed = seed * 1103515245u + 12345u;
    float unit = ((seed >> 16) & 0x7FFF) / 32768.0f; /* 0.0 - 1.0 */
    return (unit - 0.5f) * 2.0f * amplitude;
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_USART2_UART_Init();
    ai_inference_init();

    uint8_t txbuf[HDLC_TX_BUF_SIZE];
    float t = 0.0f;

    while (1)
    {
        SensorFrame_t frame;
        frame.sensor_id = SENSOR_ID;
        frame.timestamp_ms = HAL_GetTick();

        /* Simulated temperature: baseline 45C, slow drift, +/-1.5C noise
         * (representative of a motor housing / bearing temp) */
        frame.temperature_c = 45.0f + 5.0f * sinf(t * 0.05f) + pseudo_noise(1.5f);

        /* Simulated vibration RMS: baseline 0.8g, occasional spikes
         * to mimic a developing fault condition */
        float spike = (((uint32_t)t) % 20 == 0) ? 2.5f : 0.0f;
        frame.vibration_rms_g = 0.8f + 0.3f * sinf(t * 0.3f) + pseudo_noise(0.1f) + spike;

        /* Simulated pressure: baseline 101.3 kPa, small variation */
        frame.pressure_kpa = 101.3f + 0.5f * sinf(t * 0.02f) + pseudo_noise(0.2f);

        /* Feed the new sample into the sliding-window autoencoder.
         * Returns false for the first (WINDOW_SIZE - 1) samples while
         * the window fills; frame.anomaly_score stays 0 until then. */
        float score = 0.0f;
        uint8_t is_anomaly = 0;
        ai_inference_update(frame.temperature_c, frame.vibration_rms_g,
                             frame.pressure_kpa, &score, &is_anomaly);
        frame.anomaly_score = score;
        frame.is_anomaly = is_anomaly;

        uint16_t len = hdlc_encode_frame(&frame, txbuf, sizeof(txbuf));
        if (len > 0)
        {
            HAL_UART_Transmit(&huart2, txbuf, len, HAL_MAX_DELAY);
        }

        t += (float)SAMPLE_PERIOD_MS / 1000.0f;
        HAL_Delay(SAMPLE_PERIOD_MS);
    }
}

static void MX_USART2_UART_Init(void)
{
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK)
    {
        Error_Handler();
    }
}

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                 | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
    {
        Error_Handler();
    }
}

void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}
