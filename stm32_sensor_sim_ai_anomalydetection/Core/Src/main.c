/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "sensor_protocol.h"
#include "ai_inference.h"
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "mpu6050.h"
#include "vibration_rms.h"
#include "vibration_itm_logger.h"
#include "ota_receiver.h"
#include "ota_confirm.h"
/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

static hdlc_decoder_t ota_decoder;
static uint8_t ota_rx_byte;

typedef enum {
    OTA_PENDING_NONE = 0,
    OTA_PENDING_BEGIN,
} ota_pending_action_t;

static volatile ota_pending_action_t ota_pending_action = OTA_PENDING_NONE;
static uint32_t ota_pending_total_size;

static uint32_t ota_boot_tick;
static bool ota_confirmed = false;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);
/* USER CODE BEGIN PFP */

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

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();
  ota_receiver_init();
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
  ai_inference_init();
  MPU6050_Init();
  itm_init();

  /* OTA: initialize the HDLC decoder and start interrupt-driven
   * single-byte UART RX so the ESP32 can send OTA command frames
   * (BEGIN/DATA/END) asynchronously, independent of the TX-only
   * sensor telemetry loop below. */
  hdlc_decoder_init(&ota_decoder);
  HAL_UART_Receive_IT(&huart2, &ota_rx_byte, 1);
  ota_boot_tick = HAL_GetTick();

  uint8_t txbuf[HDLC_TX_BUF_SIZE];
  float t = 0.0f;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  	  	  /* OTA: handle a deferred BEGIN here in the main loop, not
	  	  	   * inside the UART RX interrupt -- this is where the ~1s
	  	  	   * blocking sector erase actually happens. Sensor sampling
	  	  	   * pauses for that ~1s while this runs, which only happens
	  	  	   * during an actual firmware update, not steady-state. */
	  	  	  if (ota_pending_action == OTA_PENDING_BEGIN) {
	  	  	      ota_result_t r = ota_receiver_begin(ota_pending_total_size);
	  	  	      uint8_t resp = (r == OTA_OK) ? OTA_ACK_BYTE : OTA_NAK_BYTE;
	  	  	      uint8_t ackbuf[8];
	  	  	      uint16_t acklen = hdlc_encode_raw(&resp, 1, ackbuf, sizeof(ackbuf));
	  	  	      if (acklen > 0) HAL_UART_Transmit(&huart2, ackbuf, acklen, HAL_MAX_DELAY);
	  	  	      ota_pending_action = OTA_PENDING_NONE;
	  	  	  }

	  	  	  /* OTA: a few seconds after boot, confirm this image is good
	  	  	   * so the bootloader stops counting boot attempts against it.
	  	  	   * Simple time-based check for now -- tie this to a real
	  	  	   * self-check (AI inference producing valid output, sensor
	  	  	   * responding, etc) once you have one defined. */
	  	  	  if (!ota_confirmed && (HAL_GetTick() - ota_boot_tick > 10000)) {
	  	  	      ota_confirm_good();
	  	  	      ota_confirmed = true;
	  	  	  }

	  	  	  //vibration_logging_task();
	  	  	  SensorFrame_t frame;
	          frame.sensor_id = SENSOR_ID;
	          frame.timestamp_ms = HAL_GetTick();

	          /* Simulated temperature: baseline 45C, slow drift, +/-1.5C noise
	           * (representative of a motor housing / bearing temp) */
	          frame.temperature_c = 45.0f + 5.0f * sinf(t * 0.05f) + pseudo_noise(1.5f);

	          /* Simulated vibration RMS: baseline 0.8g, occasional spikes
	           * to mimic a developing fault condition */
	          //float spike = (((uint32_t)t) % 20 == 0) ? 2.5f : 0.0f;
	          //frame.vibration_rms_g = 0.8f + 0.3f * sinf(t * 0.3f) + pseudo_noise(0.1f) + spike;
	          frame.vibration_rms_g = vibration_rms_read();
	          /* Simulated pressure: baseline 101.3 kPa, small variation */
	          //frame.pressure_kpa = 101.3f + 0.5f * sinf(t * 0.02f) + pseudo_noise(0.2f);
              frame.pressure_kpa = 250;
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
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
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
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* Sends a single-byte ACK or NAK frame back to the ESP32, HDLC-framed
 * and CRC-checked the same as everything else on this link. Used for
 * OTA_CMD_DATA and OTA_CMD_END responses -- OTA_CMD_BEGIN's response
 * is sent separately from the main loop instead, since it has to wait
 * for the deferred erase to actually finish first (see main loop). */
static void ota_send_ack(void)
{
    uint8_t resp = OTA_ACK_BYTE;
    uint8_t buf[8];
    uint16_t len = hdlc_encode_raw(&resp, 1, buf, sizeof(buf));
    if (len > 0) HAL_UART_Transmit(&huart2, buf, len, HAL_MAX_DELAY);
}

static void ota_send_nak(void)
{
    uint8_t resp = OTA_NAK_BYTE;
    uint8_t buf[8];
    uint16_t len = hdlc_encode_raw(&resp, 1, buf, sizeof(buf));
    if (len > 0) HAL_UART_Transmit(&huart2, buf, len, HAL_MAX_DELAY);
}

/* Called once a complete, CRC-verified OTA frame has been decoded.
 * payload[0] is always the command byte (OTA_CMD_BEGIN/DATA/END --
 * see ota_receiver.h). Runs in interrupt context (called from
 * HAL_UART_RxCpltCallback below) for everything except BEGIN, whose
 * actual erase work is deferred to the main loop since it blocks for
 * ~1s -- far too long to spend inside an ISR. */
static void ota_dispatch_frame(const uint8_t *payload, uint16_t len)
{
    if (len < 1) { ota_send_nak(); return; }

    uint8_t cmd = payload[0];

    switch (cmd) {

    case OTA_CMD_BEGIN: {
        if (len < 1 + sizeof(uint32_t)) { ota_send_nak(); return; }
        uint32_t total_size;
        memcpy(&total_size, &payload[1], sizeof(uint32_t));
        /* Defer to main loop -- do NOT call ota_receiver_begin() here,
         * it blocks for ~1s doing sector erase. No ACK/NAK sent yet;
         * the main loop sends it once the erase actually completes. */
        ota_pending_total_size = total_size;
        ota_pending_action = OTA_PENDING_BEGIN;
        break;
    }

    case OTA_CMD_DATA: {
        if (len < 1 + 4 + 2) { ota_send_nak(); return; }
        uint32_t offset;
        uint16_t chunk_len;
        memcpy(&offset, &payload[1], sizeof(uint32_t));
        memcpy(&chunk_len, &payload[5], sizeof(uint16_t));
        const uint8_t *data = &payload[7];

        if (7 + chunk_len > len) { ota_send_nak(); return; }

        ota_result_t r = ota_receiver_data(offset, data, chunk_len);
        if (r == OTA_OK) ota_send_ack(); else ota_send_nak();
        break;
    }

    case OTA_CMD_END: {
        if (len < 1 + sizeof(uint32_t)) { ota_send_nak(); return; }
        uint32_t crc;
        memcpy(&crc, &payload[1], sizeof(uint32_t));

        ota_result_t r = ota_receiver_end(crc);
        if (r == OTA_OK) {
            /* ACK first so the ESP32 knows the transfer genuinely
             * succeeded, THEN reboot into the bootloader. */
            ota_send_ack();
            ota_reboot_to_apply_update(); /* never returns */
        } else {
            ota_send_nak();
        }
        break;
    }

    default:
        ota_send_nak();
        break;
    }
}

/* HAL calls this once the single byte armed by HAL_UART_Receive_IT
 * has arrived. Feed it to the HDLC decoder, dispatch a complete frame
 * if one just finished, then immediately re-arm for the next byte --
 * missing the re-arm would silently stop all further RX. */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        int16_t result = hdlc_decoder_feed(&ota_decoder, ota_rx_byte);
        if (result > 0) {
            ota_dispatch_frame(ota_decoder.buf, (uint16_t)result);
        }
        /* result == 0: frame still in progress, nothing to do.
         * result < 0: CRC error or overflow, decoder already reset
         * itself -- nothing to do here either, just keep receiving. */

        HAL_UART_Receive_IT(&huart2, &ota_rx_byte, 1);
    }
}

/* HAL calls this on any UART error (overrun, framing, noise, parity).
 * Critically: HAL_UART_Receive_IT does NOT automatically re-arm itself
 * after an error -- it aborts the receive and resets RxState, but
 * never calls HAL_UART_RxCpltCallback (only a successful receive does
 * that). Without this callback re-arming reception explicitly, a
 * SINGLE glitch on the RX line (very plausible right at power-up, or
 * from any electrical noise) would silently and permanently stop all
 * future UART RX -- while TX keeps working fine, since it's a
 * separate state machine (gState vs RxState). This exact asymmetry
 * -- telemetry TX still flowing, RX completely dead -- is what made
 * this bug easy to miss until an OTA command actually needed the RX
 * path to still be alive. */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        __HAL_UART_CLEAR_PEFLAG(huart); /* clear parity/framing/noise/overrun flags */
        HAL_UART_Receive_IT(&huart2, &ota_rx_byte, 1);
    }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
