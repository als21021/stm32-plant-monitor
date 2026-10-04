#include "led.h"
#include "main.h"

/* 가정: CubeMX User Label = GROW_LED (현재 .ioc는 GROW_LEDs, 라벨 확정 후 수정) */

app_status_t led_init(void)
{
    /* TODO(양상진): OFF 출력 */
    return APP_OK;
}

void led_set(bool on)
{
    /* TODO(양상진): 릴레이/트랜지스터 극성에 맞게 출력 */
    (void)on;
}

bool led_is_on(void)
{
    return false;
}
