#include "soil_sensor.h"
#include <stdbool.h>
#include "main.h"
#include "config.h"

extern ADC_HandleTypeDef hadc1;   /* 가정: ADC1, 12비트, 단일 채널, 연속 변환 OFF */

#define SOIL_ADC_TIMEOUT_MS  10U  /* 1회 변환 대기 상한 (튜닝 값 아님) */

/* 이동평균용 링 버퍼 (FR-01) */
static uint16_t sample_buffer[SOIL_MOVING_AVG_COUNT];
static uint8_t  sample_index = 0;
static uint8_t  sample_count = 0;
static uint32_t sample_sum = 0;

static app_status_t read_adc_once(uint16_t *sample)
{
    if (HAL_ADC_Start(&hadc1) != HAL_OK)
    {
        return APP_ERR_NOT_READY;
    }

    if (HAL_ADC_PollForConversion(&hadc1, SOIL_ADC_TIMEOUT_MS) != HAL_OK)
    {
        HAL_ADC_Stop(&hadc1);
        return APP_ERR_TIMEOUT;
    }

    *sample = (uint16_t)HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    return APP_OK;
}

/* 정상 범위: [WET - MARGIN, DRY + MARGIN] (FR-11, SR-02) */
static bool is_in_range(uint16_t sample)
{
    int32_t low  = (int32_t)SOIL_RAW_WET - (int32_t)SOIL_RAW_MARGIN;
    int32_t high = (int32_t)SOIL_RAW_DRY + (int32_t)SOIL_RAW_MARGIN;

    return ((int32_t)sample >= low) && ((int32_t)sample <= high);
}

static void push_sample(uint16_t sample)
{
    if (sample_count == SOIL_MOVING_AVG_COUNT)
    {
        sample_sum -= sample_buffer[sample_index];   /* 가장 오래된 값 제거 */
    }
    else
    {
        sample_count++;
    }

    sample_buffer[sample_index] = sample;
    sample_sum += sample;
    sample_index = (uint8_t)((sample_index + 1U) % SOIL_MOVING_AVG_COUNT);
}

/* 2점 선형 변환 + 0~100 클램핑 (FR-02). 정전용량식은 마를수록 값이 크다 */
static uint8_t raw_to_percent(uint16_t raw)
{
    int32_t span = (int32_t)SOIL_RAW_DRY - (int32_t)SOIL_RAW_WET;
    int32_t percent = ((int32_t)SOIL_RAW_DRY - (int32_t)raw) * 100 / span;

    if (percent < 0)
    {
        percent = 0;
    }
    else if (percent > 100)
    {
        percent = 100;
    }
    return (uint8_t)percent;
}

app_status_t soil_sensor_init(void)
{
    /* F4 계열 ADC는 별도 캘리브레이션 함수가 없으므로 버퍼만 초기화 */
    sample_index = 0;
    sample_count = 0;
    sample_sum = 0;
    return APP_OK;
}

/*
 * 반환값별 출력:
 * - APP_OK        : raw_value = 이동평균값, moisture_percent = 변환값
 * - APP_ERR_RANGE : raw_value = 이번 순간값(로그·캘리브레이션 확인용), percent는 변경 안 함
 * - 그 외 에러    : 둘 다 변경 안 함
 * 범위 밖 샘플은 평균 버퍼에 넣지 않는다 (복구 후 평균 오염 방지)
 */
app_status_t soil_sensor_read(uint16_t *raw_value, uint8_t *moisture_percent)
{
    uint16_t sample = 0;
    app_status_t status = read_adc_once(&sample);

    if (status != APP_OK)
    {
        return status;
    }

    if (!is_in_range(sample))
    {
        *raw_value = sample;
        return APP_ERR_RANGE;
    }

    push_sample(sample);

    uint16_t average = (uint16_t)(sample_sum / sample_count);
    *raw_value = average;
    *moisture_percent = raw_to_percent(average);
    return APP_OK;
}
