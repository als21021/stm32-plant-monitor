#include "pump.h"
#include "main.h"
#include "config.h"

/* 가정: CubeMX User Label = PUMP_RELAY (핀 라벨 확정 후 수정) */

app_status_t pump_init(void)
{
    /* TODO(김민산): 릴레이 OFF 레벨 출력 (PUMP_RELAY_ACTIVE_LOW 고려) */
    return APP_OK;
}

app_status_t pump_start(uint32_t duration_ms, uint32_t now_ms)
{
    /* TODO(김민산): 상한 적용, ON 출력, 시작 시각·지속 시간 저장 */
    (void)duration_ms;
    (void)now_ms;
    return APP_ERR_NOT_READY;
}

void pump_stop(void)
{
    /* TODO(김민산): OFF 출력, 상태 초기화 */
}

void pump_update(uint32_t now_ms)
{
    /* TODO(김민산): (now_ms - start) >= duration 이면 pump_stop() */
    (void)now_ms;
}

bool pump_is_running(void)
{
    return false;
}
