#include "watering.h"
#include "pump.h"
#include "config.h"

void watering_init(void)
{
    /* TODO(김민산): 상태·카운터 초기화 */
}

void watering_update(app_status_t soil_status, uint8_t moisture_percent, uint32_t now_ms)
{
    /* TODO(김민산): 상태 머신 구현 (watering_state_t 참고) */
    (void)soil_status;
    (void)moisture_percent;
    (void)now_ms;
}

watering_state_t watering_get_state(void)
{
    return WATERING_IDLE;
}
