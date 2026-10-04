#ifndef LIGHTING_H
#define LIGHTING_H

#include <stdint.h>
#include "app_status.h"

/* 담당: 양상진 */

void lighting_init(void);

/*
 * 조도 측정 직후 호출. LIGHT_ON_LUX 미만이면 ON, LIGHT_OFF_LUX 초과면 OFF (FR-06).
 * 센서 에러면 LED 상태를 유지한다 (FR-10).
 */
void lighting_update(app_status_t light_status, uint16_t lux);

#endif /* LIGHTING_H */
