#ifndef LED_H
#define LED_H

#include <stdbool.h>
#include "app_status.h"

/* 담당: 양상진 */

/* LED OFF 상태로 초기화 */
app_status_t led_init(void);

void led_set(bool on);

bool led_is_on(void);

#endif /* LED_H */
