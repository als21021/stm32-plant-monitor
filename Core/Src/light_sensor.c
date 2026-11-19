/*
 * light_sensor.c
 *
 * BH1750 조도 센서 (I2C) 드라이버
 * 핀 번호는 코드에 없고, CubeMX가 생성한 hi2c1 설정을 그대로 사용함
 */

#include "light_sensor.h"
#include "i2c.h"    // hi2c1 핸들 선언

/* 타이밍 관리용 변수 (이 파일 안에서만 사용) */
static uint32_t light_start_tick = 0;   // 기준 시각 (ms)
static uint32_t light_wait_ms    = 0;   // 기준 시각으로부터 기다릴 시간 (ms)

HAL_StatusTypeDef light_sensor_init(void)
{
    uint8_t cmd = BH1750_CMD_POWER_ON;
    HAL_StatusTypeDef ret;

    /* 기준 시각 기록: 첫 측정이 끝날 때까지 LIGHT_MEASURE_WAIT_MS 기다림
     * (init이 실패해도 타이밍은 동작하도록 통신 전에 먼저 기록) */
    light_start_tick = HAL_GetTick();
    light_wait_ms    = LIGHT_MEASURE_WAIT_MS;

    /* 1단계: 전원 ON 명령 전송 */
    ret = HAL_I2C_Master_Transmit(&hi2c1, LIGHT_I2C_ADDR, &cmd, 1, LIGHT_I2C_TIMEOUT_MS);
    if (ret != HAL_OK)
    {
        return ret;     // 센서 응답 없음 (배선, 주소, 풀업 저항 확인)
    }

    /* 2단계: 연속 고해상도 측정 모드 시작 */
    cmd = BH1750_CMD_CONT_H;
    ret = HAL_I2C_Master_Transmit(&hi2c1, LIGHT_I2C_ADDR, &cmd, 1, LIGHT_I2C_TIMEOUT_MS);
    if (ret != HAL_OK)
    {
        return ret;
    }

    return HAL_OK;      // 여기서 기다리지 않음 (대기는 period_elapsed에서 처리)
}

bool light_sensor_period_elapsed(void)
{
    /* 현재 시각 - 기준 시각 >= 기다릴 시간 이면 읽을 차례
     * 부호 없는 뺄셈이라 약 49.7일 뒤 tick이 한 바퀴 돌아도 정상 동작함 */
    if ((HAL_GetTick() - light_start_tick) >= light_wait_ms)
    {
        light_start_tick = HAL_GetTick();        // 다음 주기의 기준 시각 갱신
        light_wait_ms    = LIGHT_READ_PERIOD_MS; // 이후부터는 읽기 주기 간격
        return true;
    }

    return false;
}

HAL_StatusTypeDef light_sensor_read(float *lux)
{
    uint8_t buf[2];
    HAL_StatusTypeDef ret;

    /* 센서에서 2바이트 수신 (상위 바이트, 하위 바이트 순서) */
    ret = HAL_I2C_Master_Receive(&hi2c1, LIGHT_I2C_ADDR, buf, 2, LIGHT_I2C_TIMEOUT_MS);
    if (ret != HAL_OK)
    {
        return ret;     // 실패 시 lux 값은 건드리지 않음
    }

    /* 2바이트를 16비트 값으로 합치기 */
    uint16_t raw = (uint16_t)((buf[0] << 8) | buf[1]);

    /* 원시값을 lux로 변환 (데이터시트 기준 raw / 1.2) */
    *lux = raw / BH1750_LUX_DIVIDER;

    return HAL_OK;
}
