#include "soil_sensor.h"   /* 이 모듈이 공개하는 함수 선언 */
#include <stdbool.h>       /* bool, true, false 사용 */
#include "main.h"          /* HAL 함수와 ADC_HandleTypeDef 정의 */
#include "config.h"        /* SOIL_RAW_DRY, SOIL_RAW_WET 등 튜닝 상수 */

/* hadc1은 main.c에서 정의됨. extern은 "다른 파일에 있는 걸 같이 쓴다"는 선언 */
extern ADC_HandleTypeDef hadc1;   /* 가정: ADC1, 12비트(0~4095), 단일 채널, 연속 변환 OFF */

#define SOIL_ADC_TIMEOUT_MS  10U  /* 1회 변환 대기 상한 (튜닝 값 아님). U = unsigned 상수 */

/*
 * 이동평균용 링 버퍼 (FR-01)
 * static 전역: 이 파일 안에서만 보이고, 함수가 끝나도 값이 유지됨
 */
static uint16_t sample_buffer[SOIL_MOVING_AVG_COUNT]; /* 최근 측정값 저장 칸 */
static uint8_t  sample_index = 0;  /* 다음에 쓸 칸 번호 */
static uint8_t  sample_count = 0;  /* 지금까지 채운 칸 수 (최대 SOIL_MOVING_AVG_COUNT) */
static uint32_t sample_sum = 0;    /* 버퍼 값 합계. 여러 값을 더하므로 32비트 */

/*
 * ADC를 한 번 변환해서 *sample에 저장
 * 반환값으로 성공/실패를 알리고, 측정값은 포인터로 돌려줌
 */
static app_status_t read_adc_once(uint16_t *sample)
{
    /* 변환 시작 */
    if (HAL_ADC_Start(&hadc1) != HAL_OK)
    {
        return APP_ERR_NOT_READY;
    }

    /* 변환 완료까지 대기(폴링). 타임아웃이면 ADC를 끄고 에러 반환 → 무한 대기 방지 */
    if (HAL_ADC_PollForConversion(&hadc1, SOIL_ADC_TIMEOUT_MS) != HAL_OK)
    {
        HAL_ADC_Stop(&hadc1);
        return APP_ERR_TIMEOUT;
    }

    /* 결과는 32비트로 오지만 12비트 값이므로 16비트로 캐스팅 */
    *sample = (uint16_t)HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);   /* 단발 변환이므로 매번 정지 */
    return APP_OK;
}

/*
 * 정상 범위: [WET - MARGIN, DRY + MARGIN] (FR-11, SR-02)
 * 범위 밖이면 선 빠짐·센서 고장으로 판단
 */
static bool is_in_range(uint16_t sample)
{
    /*
     * int32_t로 바꿔서 계산하는 이유:
     * unsigned끼리 빼서 음수가 되면 큰 양수로 뒤집힘(언더플로)
     */
    int32_t low  = (int32_t)SOIL_RAW_WET - (int32_t)SOIL_RAW_MARGIN;
    int32_t high = (int32_t)SOIL_RAW_DRY + (int32_t)SOIL_RAW_MARGIN;

    return ((int32_t)sample >= low) && ((int32_t)sample <= high);
}

/* 링 버퍼에 새 샘플 추가 + 합계 갱신 (매번 전체를 다시 더하지 않음) */
static void push_sample(uint16_t sample)
{
    if (sample_count == SOIL_MOVING_AVG_COUNT)
    {
        /* 버퍼가 가득 참: 이번에 덮어쓸 칸이 가장 오래된 값이므로 합계에서 뺌 */
        sample_sum -= sample_buffer[sample_index];
    }
    else
    {
        sample_count++;   /* 아직 덜 참: 채운 칸 수만 증가 */
    }

    sample_buffer[sample_index] = sample;
    sample_sum += sample;

    /* 마지막 칸 다음은 0번으로 돌아감 (0, 1, ..., N-1, 0, 1, ...) */
    sample_index = (uint8_t)((sample_index + 1U) % SOIL_MOVING_AVG_COUNT);
}

/*
 * 2점 선형 변환 + 0~100 클램핑 (FR-02)
 * 정전용량식은 마를수록 값이 크다 → DRY = 0%, WET = 100%
 * 예) DRY 3000, WET 1300, raw 2150 → (3000 - 2150) * 100 / 1700 = 50%
 */
static uint8_t raw_to_percent(uint16_t raw)
{
    int32_t span = (int32_t)SOIL_RAW_DRY - (int32_t)SOIL_RAW_WET;

    /* 100을 먼저 곱하고 나눔: 정수 나눗셈은 소수점을 버리므로 먼저 나누면 0이 됨 */
    int32_t percent = ((int32_t)SOIL_RAW_DRY - (int32_t)raw) * 100 / span;

    /* MARGIN 허용 때문에 0 미만 / 100 초과가 나올 수 있어 잘라냄 */
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

    /* ADC 읽기 실패: 에러를 그대로 호출자에게 전달 */
    if (status != APP_OK)
    {
        return status;
    }

    /* 범위 판정은 평균이 아닌 순간값으로 → 선이 빠지면 다음 측정에서 바로 감지 */
    if (!is_in_range(sample))
    {
        *raw_value = sample;
        return APP_ERR_RANGE;
    }

    push_sample(sample);

    /*
     * 채운 칸 수로 나눠서 시작 직후에도 정확한 평균
     * push_sample 이후라 sample_count >= 1 → 0으로 나눌 일 없음
     */
    uint16_t average = (uint16_t)(sample_sum / sample_count);
    *raw_value = average;
    *moisture_percent = raw_to_percent(average);
    return APP_OK;
}
