/*
 * light_sensor.h
 *
 * BH1750 조도 센서 (I2C) 드라이버
 * 주소, 타임아웃, 대기 시간, 읽기 주기는 config.h에서 관리하고,
 * 센서 고유의 값(명령어, 변환 계수)만 이 파일에 둠
 * HAL_Delay 대신 호출자가 넘긴 now_ms 기반 논블로킹 타이밍 사용 (NFR-04)
 */

#ifndef INC_LIGHT_SENSOR_H_
#define INC_LIGHT_SENSOR_H_

#include <stdint.h>
#include <stdbool.h>    // bool 타입 사용
#include "app_status.h" // 공통 반환값 app_status_t
#include "config.h"     // LIGHT_I2C_ADDR, LIGHT_READ_PERIOD_MS 등

/* BH1750 명령어 (데이터시트로 정해진 값이라 바뀌지 않음) */
#define BH1750_CMD_POWER_ON     0x01    // 전원 ON
#define BH1750_CMD_CONT_H       0x10    // 연속 고해상도 측정 (1 lx 단위, 약 120~180ms)

/* 원시값(raw) → lux 변환: lux = raw / 1.2 = raw * 10 / 12 (데이터시트 기준)
 * float 대신 정수 연산. 최대 65535 * 10 / 12 = 54612 lx라 uint16_t에 들어감 */
#define BH1750_LUX_NUMERATOR    10U
#define BH1750_LUX_DENOMINATOR  12U

/**
 * @brief  센서 초기화 (전원 ON + 연속 측정 모드 시작)
 * @note   MX_I2C1_Init() 이후에 한 번만 호출. 첫 측정 대기는 기다리지 않고
 *         시작 시각만 기록함 (대기는 light_sensor_period_elapsed가 처리)
 * @param  now_ms: 현재 시각 (HAL_GetTick() 값을 호출자가 넘김)
 * @retval APP_OK: 성공, APP_ERR_TIMEOUT: I2C 타임아웃, APP_ERR_COMM: 그 외 I2C 실패
 */
app_status_t light_sensor_init(uint32_t now_ms);

/**
 * @brief  읽기 주기가 지났는지 확인 (논블로킹)
 * @note   while 루프에서 계속 호출하면 됨.
 *         true를 반환하는 순간 다음 주기의 시작 시각을 now_ms로 갱신함.
 *         처음에는 LIGHT_MEASURE_WAIT_MS 후, 이후에는 LIGHT_READ_PERIOD_MS마다 true (FR-03)
 * @param  now_ms: 현재 시각 (HAL_GetTick() 값을 호출자가 넘김)
 * @retval true: 지금 읽을 차례, false: 아직 아님
 */
bool light_sensor_period_elapsed(uint32_t now_ms);

/**
 * @brief  현재 조도 값 읽기
 * @param  lux: 읽은 조도 값(lx)을 저장할 변수의 포인터
 * @retval APP_OK: 성공, APP_ERR_TIMEOUT: I2C 타임아웃, APP_ERR_COMM: 그 외 I2C 실패
 *         (실패 시 *lux는 변경되지 않음)
 */
app_status_t light_sensor_read(uint16_t *lux);

#endif /* INC_LIGHT_SENSOR_H_ */
