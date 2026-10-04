#include "soil_sensor.h"
#include "main.h"
#include "config.h"

extern ADC_HandleTypeDef hadc1;   /* 가정: ADC1 사용 */

app_status_t soil_sensor_init(void)
{
    /* TODO(김민산): 필요 시 ADC 캘리브레이션/채널 확인 */
    return APP_OK;
}

app_status_t soil_sensor_read(uint16_t *raw_value, uint8_t *moisture_percent)
{
    /* TODO(김민산): HAL_ADC_Start → PollForConversion → GetValue → 범위 검사 → % 변환 */
    *raw_value = 0;
    *moisture_percent = 0;
    return APP_ERR_NOT_READY;
}
