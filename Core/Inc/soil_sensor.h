#ifndef SOIL_SENSOR_H
#define SOIL_SENSOR_H

#include <stdint.h>
#include "app_status.h"

/* 담당: 김민산 */

app_status_t soil_sensor_init(void);

/*
 * ADC 1회 변환 후 raw 값과 습도(%)를 돌려준다.
 * - 즉시 반환 (변환 대기는 짧은 타임아웃 폴링만 허용, NFR-04)
 * - raw는 SOIL_MOVING_AVG_COUNT회 이동평균 적용값 (FR-01)
 * - %는 2점 캘리브레이션 선형 변환 후 0~100 클램핑 (FR-02)
 * - raw가 [SOIL_RAW_WET - MARGIN, SOIL_RAW_DRY + MARGIN] 밖이면 APP_ERR_RANGE (FR-11)
 * - 에러여도 raw_value는 채워서 로그에 남길 수 있게 한다
 */
app_status_t soil_sensor_read(uint16_t *raw_value, uint8_t *moisture_percent);

#endif /* SOIL_SENSOR_H */
