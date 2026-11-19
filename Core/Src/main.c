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
#include "adc.h"
#include "i2c.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "uart_printf.h"
#include "watchdog.h"
#include "config.h"
#include "soil_sensor.h"
#include "pump.h"
#include "watering.h"
/* USER CODE END Includes */

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

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static const char *app_status_str(app_status_t s)
{
  switch (s)
  {
  case APP_OK:            return "OK";
  case APP_ERR_NOT_READY: return "NOT_READY";
  case APP_ERR_TIMEOUT:   return "TIMEOUT";
  case APP_ERR_RANGE:     return "RANGE";
  case APP_ERR_COMM:      return "COMM";
  default:                return "?";
  }
}

static const char *watering_state_str(watering_state_t s)
{
  switch (s)
  {
  case WATERING_IDLE:         return "IDLE";
  case WATERING_PUMPING:      return "PUMPING";
  case WATERING_SOAK_WAIT:    return "SOAK_WAIT";
  case WATERING_SENSOR_ERROR: return "SENSOR_ERROR";
  case WATERING_LOCKED_LIMIT: return "LOCKED_LIMIT";
  case WATERING_LOCKED_FAIL:  return "LOCKED_FAIL";
  default:                    return "?";
  }
}

/*
 * 급수 흐름 (FR-01, FR-04, FR-05, FR-11, FR-12, SR-02, SR-03)
 * SOIL_READ_PERIOD_MS마다 토양 센서 측정 → watering 판단 → 측정 주기 로그 한 줄
 * 로그 형식은 FR-07(SD카드) 도입 시 그대로 옮길 수 있게 한 줄로 유지
 */
static void watering_task(uint32_t now_ms)
{
  static uint32_t last_read_ms = 0;
  static bool     first_read   = true;
  static uint8_t  moisture     = 0;   /* 에러 시에는 마지막 정상값 유지 (로그용) */
  static watering_state_t last_state = WATERING_IDLE;

  if (!first_read && (uint32_t)(now_ms - last_read_ms) < SOIL_READ_PERIOD_MS)
  {
    return;
  }
  first_read = false;
  last_read_ms = now_ms;

  uint16_t raw = 0;
  app_status_t soil_status = soil_sensor_read(&raw, &moisture);
  watering_update(soil_status, moisture, now_ms);

  watering_state_t state = watering_get_state();
  if (state != last_state)
  {
    printf("[WATER] %s -> %s\r\n", watering_state_str(last_state), watering_state_str(state));
    last_state = state;
  }

  printf("[LOG] t=%lu soil=%u%% raw=%u (%s) water=%s pump=%s\r\n",
         (unsigned long)now_ms, (unsigned)moisture, (unsigned)raw,
         app_status_str(soil_status), watering_state_str(state),
         pump_is_running() ? "ON" : "OFF");
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_SPI2_Init();
  /* USER CODE BEGIN 2 */
  /* SR-01: 다른 초기화보다 먼저 펌프 OFF 확정 */
  pump_init();

  uart_printf_init();
  printf("\r\n[BOOT] plant monitor start\r\n");

  if (soil_sensor_init() != APP_OK)
  {
    printf("[SOIL] init failed\r\n");   /* 측정 실패는 watering이 SENSOR_ERROR로 처리 (SR-02) */
  }
  watering_init();

  /* 리셋 원인 확인 + 연속 IWDG 리셋 횟수 갱신 (watchdog_start보다 먼저) */
  watchdog_check_reset_cause();
  if (watchdog_was_iwdg_reset())
  {
    printf("[WDG] IWDG reset detected\r\n");
  }
  printf("[WDG] IWDG reset count: %lu\r\n", watchdog_get_reset_count());
  if (watchdog_is_reset_limit_reached())
  {
    /* TODO: 안전 모드 동작 미정 (팀 논의 필요) */
    printf("[WDG] reset limit reached\r\n");
  }

  /* 모든 초기화가 끝난 뒤 IWDG 시작 (이후 약 2초 안에 kick 필요) */
  watchdog_start();
  printf("[WDG] IWDG started\r\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint32_t now_ms = HAL_GetTick();

    /* 펌프 OFF 타이밍은 매 루프 확인 (FR-04, PUMP_MAX_ON_MS 상한) */
    pump_update(now_ms);
    watering_task(now_ms);

    /* TODO: 보광 흐름 (light_sensor → lighting → led)은 조도 센서 API 정리 후 연결 (상진) */

    /* 워치독 리프레시: 루프가 2초 이상 멈추면 IWDG 리셋 발생 */
    watchdog_kick();
    /* 1시간 정상 동작 시 연속 리셋 횟수 초기화 */
    watchdog_check_stable_time();
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 100;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

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
