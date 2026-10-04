#ifndef PUMP_H
#define PUMP_H

#include <stdint.h>
#include <stdbool.h>
#include "app_status.h"

/* 담당: 김민산 */

/* 펌프 OFF 상태로 초기화 (SR-01: 리셋 직후 펌프 OFF 보장) */
app_status_t pump_init(void);

/*
 * 펌프를 켜고 즉시 반환한다. duration_ms 후 pump_update()가 자동으로 끈다.
 * - duration_ms는 PUMP_MAX_ON_MS로 잘라낸다 (FR-04 안전 상한)
 * - 이미 동작 중이면 무시하고 APP_ERR_NOT_READY 반환
 */
app_status_t pump_start(uint32_t duration_ms, uint32_t now_ms);

void pump_stop(void);

/* main 루프에서 매 회 호출. 시간이 다 되면 OFF */
void pump_update(uint32_t now_ms);

bool pump_is_running(void);

#endif /* PUMP_H */
