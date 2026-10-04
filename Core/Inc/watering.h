#ifndef WATERING_H
#define WATERING_H

#include <stdint.h>
#include "app_status.h"

/* 담당: 김민산 */

typedef enum {
    WATERING_IDLE = 0,
    WATERING_PUMPING,         /* 펌프 동작 중 */
    WATERING_SOAK_WAIT,       /* 흡수 대기: 재급수 금지 + 실패 판정 대기 (FR-05, FR-12) */
    WATERING_SENSOR_ERROR,    /* 토양 센서 이상 → 펌프 금지 (SR-02, FR-11) */
    WATERING_LOCKED_LIMIT,    /* 시간당 횟수 초과 → 리셋 전까지 정지 (SR-03) */
    WATERING_LOCKED_FAIL      /* 연속 급수 실패 → 정지 (FR-12) */
} watering_state_t;

void watering_init(void);

/*
 * 토양 센서 측정 직후 호출. 판단 결과에 따라 pump_start()를 부른다.
 * HAL을 직접 호출하지 않는다 (드라이버 함수만 사용).
 */
void watering_update(app_status_t soil_status, uint8_t moisture_percent, uint32_t now_ms);

watering_state_t watering_get_state(void);

#endif /* WATERING_H */
