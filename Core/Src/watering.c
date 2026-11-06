#include "watering.h"
#include "pump.h"
#include "config.h"

/*
 * 급수 판단 상태 머신 (FR-04, FR-05, FR-11, FR-12, SR-02, SR-03)
 * HAL을 직접 부르지 않고 pump 드라이버 함수만 사용한다.
 * 시간은 호출자가 넘긴 now_ms만 쓰고, 경과 비교는 (now - start >= period) 형태로 한다.
 *
 *   IDLE ──습도 < 하한──▶ PUMPING ──펌프 OFF──▶ SOAK_WAIT ──대기 끝──▶ IDLE
 *     │                     │                         │
 *     │ 센서 이상           │ 센서 이상 (즉시 OFF)    │ 대기 끝에 센서 이상
 *     ▼                     ▼                         ▼
 *   SENSOR_ERROR ──센서 정상 복귀──▶ IDLE
 *
 *   IDLE에서 시간당 횟수 초과        → LOCKED_LIMIT (리셋 전까지)
 *   SOAK_WAIT 끝에 연속 실패 N회     → LOCKED_FAIL  (리셋 전까지)
 */

#define WATERING_WINDOW_MS   3600000U   /* SR-03 "시간당" 기준 구간 (1시간) */

#if PUMP_MAX_COUNT_PER_HOUR == 0
#error "PUMP_MAX_COUNT_PER_HOUR는 1 이상이어야 함"
#endif

/* static 전역: 이 파일 안에서만 보이는 급수 상태 */
static watering_state_t state = WATERING_IDLE;
static uint32_t soak_start_ms = 0;      /* 펌프가 꺼진 시각 (흡수 대기 시작) */
static uint8_t  moisture_before = 0;    /* 급수 직전 습도 (FR-12 상승폭 비교용) */
static uint8_t  fail_count = 0;         /* 연속 급수 실패 횟수 (FR-12) */

/*
 * SR-03: 최근 급수 시작 시각을 원형 버퍼로 보관
 * 버퍼가 가득 찼는데 가장 오래된 기록이 1시간 이내면, 이번 급수가 한도를 넘는 것
 */
static uint32_t start_history[PUMP_MAX_COUNT_PER_HOUR];
static uint8_t  history_count = 0;      /* 채워진 칸 수 (최대 PUMP_MAX_COUNT_PER_HOUR) */
static uint8_t  history_next = 0;       /* 다음에 쓸 칸 = 버퍼가 가득 찼을 때 가장 오래된 칸 */

static bool hourly_limit_reached(uint32_t now_ms)
{
    if (history_count < PUMP_MAX_COUNT_PER_HOUR)
    {
        return false;
    }
    return (uint32_t)(now_ms - start_history[history_next]) < WATERING_WINDOW_MS;
}

static void record_start(uint32_t now_ms)
{
    start_history[history_next] = now_ms;
    history_next = (uint8_t)((history_next + 1U) % PUMP_MAX_COUNT_PER_HOUR);
    if (history_count < PUMP_MAX_COUNT_PER_HOUR)
    {
        history_count++;
    }
}

/* 펌프를 끄고 상태 전환 → 펌프를 금지하는 상태로 갈 때는 항상 이 함수로 */
static void enter_stopped_state(watering_state_t next)
{
    pump_stop();
    state = next;
}

void watering_init(void)
{
    state = WATERING_IDLE;
    soak_start_ms = 0;
    moisture_before = 0;
    fail_count = 0;
    history_count = 0;
    history_next = 0;
}

static void update_idle(uint8_t moisture_percent, uint32_t now_ms)
{
    if (moisture_percent >= SOIL_LOW_THRESHOLD)
    {
        return;
    }

    if (hourly_limit_reached(now_ms))
    {
        enter_stopped_state(WATERING_LOCKED_LIMIT);   /* SR-03 */
        return;
    }

    /* 켜지지 않았으면(드라이버 init 전 등) 횟수에 넣지 않고 다음 주기에 다시 시도 */
    if (pump_start(PUMP_ON_MS, now_ms) == APP_OK)
    {
        moisture_before = moisture_percent;
        record_start(now_ms);
        state = WATERING_PUMPING;
    }
}

static void update_pumping(uint32_t now_ms)
{
    /* OFF는 pump_update()가 PUMP_ON_MS 경과 시 처리. 여기서는 꺼졌는지만 확인 */
    if (!pump_is_running())
    {
        soak_start_ms = now_ms;
        state = WATERING_SOAK_WAIT;
    }
}

/*
 * FR-05: 대기 중에는 급수 판정을 하지 않는다 (펌프는 이미 OFF)
 * 대기 중 센서 이상은 대기 끝에서만 반영 → 센서가 잠깐 튀어도 흡수 대기를 건너뛰지 않음
 */
static void update_soak_wait(app_status_t soil_status, uint8_t moisture_percent, uint32_t now_ms)
{
    if ((uint32_t)(now_ms - soak_start_ms) < SOAK_WAIT_MS)
    {
        return;
    }

    if (soil_status != APP_OK)
    {
        /* 이번 급수는 성공/실패 판정 없이 넘어감 (실패 횟수 유지) */
        enter_stopped_state(WATERING_SENSOR_ERROR);
        return;
    }

    /* FR-12: 상승폭이 기준 미만이면 실패. 습도가 오히려 내려간 경우도 실패 */
    if (moisture_percent >= moisture_before + WATER_FAIL_MIN_RISE)
    {
        fail_count = 0;
        state = WATERING_IDLE;
        return;
    }

    fail_count++;
    if (fail_count >= WATER_FAIL_MAX_COUNT)
    {
        enter_stopped_state(WATERING_LOCKED_FAIL);
    }
    else
    {
        state = WATERING_IDLE;
    }
}

void watering_update(app_status_t soil_status, uint8_t moisture_percent, uint32_t now_ms)
{
    switch (state)
    {
    case WATERING_IDLE:
        if (soil_status != APP_OK)
        {
            enter_stopped_state(WATERING_SENSOR_ERROR);   /* SR-02 */
        }
        else
        {
            update_idle(moisture_percent, now_ms);
        }
        break;

    case WATERING_PUMPING:
        if (soil_status != APP_OK)
        {
            enter_stopped_state(WATERING_SENSOR_ERROR);   /* SR-02: 동작 중이어도 즉시 OFF */
        }
        else
        {
            update_pumping(now_ms);
        }
        break;

    case WATERING_SOAK_WAIT:
        update_soak_wait(soil_status, moisture_percent, now_ms);
        break;

    case WATERING_SENSOR_ERROR:
        pump_stop();   /* SR-02: 에러 동안 OFF 고정 */
        if (soil_status == APP_OK)
        {
            state = WATERING_IDLE;
        }
        break;

    case WATERING_LOCKED_LIMIT:   /* SR-03: 리셋 전까지 유지 */
    case WATERING_LOCKED_FAIL:    /* FR-12: 리셋 전까지 유지 */
    default:
        pump_stop();
        break;
    }
}

watering_state_t watering_get_state(void)
{
    return state;
}
