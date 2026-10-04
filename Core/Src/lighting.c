#include "lighting.h"
#include "led.h"
#include "config.h"

void lighting_init(void)
{
    /* TODO(양상진): 초기 상태 설정 */
}

void lighting_update(app_status_t light_status, uint16_t lux)
{
    /* TODO(양상진): 에러면 유지, 아니면 임계값 ± 히스테리시스 비교 */
    (void)light_status;
    (void)lux;
}
