/*
 * watering.c 호스트(PC) 단위 테스트
 * pump 드라이버는 아래 가짜 구현으로 바꿔 끼우고, 시간은 테스트가 직접 넘긴다.
 * 실행: make -C test
 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "watering.h"
#include "pump.h"
#include "config.h"

/* ---- 가짜 pump 드라이버 ---- */
static bool         fake_running = false;
static int          fake_start_calls = 0;
static int          fake_stop_calls = 0;
static uint32_t     fake_last_duration = 0;
static app_status_t fake_start_result = APP_OK;

app_status_t pump_init(void) { return APP_OK; }

app_status_t pump_start(uint32_t duration_ms, uint32_t now_ms)
{
    (void)now_ms;
    fake_start_calls++;
    fake_last_duration = duration_ms;
    if (fake_start_result == APP_OK)
    {
        fake_running = true;
    }
    return fake_start_result;
}

void pump_stop(void)
{
    fake_stop_calls++;
    fake_running = false;
}

void pump_update(uint32_t now_ms) { (void)now_ms; }

bool pump_is_running(void) { return fake_running; }

/* 실제 드라이버에서 PUMP_ON_MS가 지나 pump_update()가 끈 상황 */
static void fake_pump_finished(void) { fake_running = false; }

static void reset_all(void)
{
    fake_running = false;
    fake_start_calls = 0;
    fake_stop_calls = 0;
    fake_last_duration = 0;
    fake_start_result = APP_OK;
    watering_init();
}

/* ---- 간단한 assert ---- */
static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { failures++; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

#define LOW   ((uint8_t)(SOIL_LOW_THRESHOLD - 10U))   /* 급수 필요 */
#define HIGH  ((uint8_t)(SOIL_LOW_THRESHOLD + 10U))   /* 급수 불필요 */

/* 실패 테스트가 SR-03 시간당 한도에 걸리지 않도록 급수 사이에 두는 간격 */
#define WATERING_TEST_GAP_MS  (3600000U / PUMP_MAX_COUNT_PER_HOUR)

/* run_cycle() 1회에 걸리는 시간 */
#define CYCLE_MS  (PUMP_ON_MS + SOAK_WAIT_MS)

/*
 * 급수 1회를 처음부터 끝까지 진행: 하한 미만 → 펌프 ON → OFF → 흡수 대기 끝
 * after: 대기 끝 시점 습도. 반환값: 대기가 끝난 시각
 */
static uint32_t run_cycle(uint32_t now, uint8_t before, uint8_t after)
{
    watering_update(APP_OK, before, now);
    fake_pump_finished();
    now += PUMP_ON_MS;
    watering_update(APP_OK, before, now);   /* PUMPING → SOAK_WAIT */
    now += SOAK_WAIT_MS;
    watering_update(APP_OK, after, now);    /* 대기 끝 판정 */
    return now;
}

/* ---- 테스트 ---- */
static void test_idle_no_water_when_moist(void)
{
    reset_all();
    CHECK(watering_get_state() == WATERING_IDLE);
    watering_update(APP_OK, HIGH, 1000);
    watering_update(APP_OK, SOIL_LOW_THRESHOLD, 2000);   /* 경계값: "미만"일 때만 급수 */
    CHECK(watering_get_state() == WATERING_IDLE);
    CHECK(fake_start_calls == 0);
}

static void test_normal_cycle(void)
{
    reset_all();
    watering_update(APP_OK, LOW, 1000);
    CHECK(watering_get_state() == WATERING_PUMPING);
    CHECK(fake_start_calls == 1);
    CHECK(fake_last_duration == PUMP_ON_MS);   /* FR-04 1회 급수 시간 */

    /* 아직 도는 중이면 PUMPING 유지 */
    watering_update(APP_OK, LOW, 2000);
    CHECK(watering_get_state() == WATERING_PUMPING);

    fake_pump_finished();
    watering_update(APP_OK, LOW, 1000 + PUMP_ON_MS);
    CHECK(watering_get_state() == WATERING_SOAK_WAIT);

    /* FR-05: 대기 중에는 습도가 낮아도 재급수 금지 */
    watering_update(APP_OK, LOW, 1000 + PUMP_ON_MS + SOAK_WAIT_MS - 1U);
    CHECK(watering_get_state() == WATERING_SOAK_WAIT);
    CHECK(fake_start_calls == 1);

    /* 대기 끝, 습도 충분히 상승 → IDLE */
    watering_update(APP_OK, (uint8_t)(LOW + WATER_FAIL_MIN_RISE), 1000 + PUMP_ON_MS + SOAK_WAIT_MS);
    CHECK(watering_get_state() == WATERING_IDLE);
}

static void test_sensor_error_in_idle(void)
{
    reset_all();
    watering_update(APP_ERR_RANGE, LOW, 1000);
    CHECK(watering_get_state() == WATERING_SENSOR_ERROR);
    CHECK(fake_start_calls == 0);
    CHECK(fake_stop_calls >= 1);   /* SR-02 */

    /* 에러 동안 습도가 낮아도 펌프 금지 */
    watering_update(APP_ERR_RANGE, LOW, 2000);
    CHECK(fake_start_calls == 0);

    /* 정상 복귀 → IDLE, 다음 주기부터 판정 */
    watering_update(APP_OK, HIGH, 3000);
    CHECK(watering_get_state() == WATERING_IDLE);
}

static void test_sensor_error_while_pumping(void)
{
    reset_all();
    watering_update(APP_OK, LOW, 1000);
    CHECK(fake_running);

    watering_update(APP_ERR_RANGE, LOW, 1500);
    CHECK(watering_get_state() == WATERING_SENSOR_ERROR);
    CHECK(!fake_running);   /* SR-02: 즉시 OFF */
}

static void test_sensor_error_during_soak(void)
{
    reset_all();
    watering_update(APP_OK, LOW, 0);
    fake_pump_finished();
    watering_update(APP_OK, LOW, PUMP_ON_MS);

    /* 대기 중 에러는 대기를 끊지 않음 (FR-05 우선) */
    watering_update(APP_ERR_RANGE, LOW, PUMP_ON_MS + 1000U);
    CHECK(watering_get_state() == WATERING_SOAK_WAIT);

    /* 대기 끝에도 에러면 SENSOR_ERROR */
    watering_update(APP_ERR_RANGE, LOW, PUMP_ON_MS + SOAK_WAIT_MS);
    CHECK(watering_get_state() == WATERING_SENSOR_ERROR);
    CHECK(fake_start_calls == 1);
}

static void test_fail_lock(void)
{
    uint32_t now = 0;
    reset_all();

    for (uint32_t i = 0; i < WATER_FAIL_MAX_COUNT; i++)
    {
        CHECK(watering_get_state() == WATERING_IDLE);
        /* 상승폭이 기준보다 1 작음 → 실패 */
        now = run_cycle(now, LOW, (uint8_t)(LOW + WATER_FAIL_MIN_RISE - 1U));
        now += WATERING_TEST_GAP_MS;
    }
    CHECK(watering_get_state() == WATERING_LOCKED_FAIL);   /* FR-12 */

    /* 잠금 후에는 습도가 낮아도 급수 안 함 */
    int calls = fake_start_calls;
    watering_update(APP_OK, LOW, now + 1000U);
    CHECK(fake_start_calls == calls);
    CHECK(watering_get_state() == WATERING_LOCKED_FAIL);
}

static void test_fail_count_reset_on_success(void)
{
    uint32_t now = 0;
    reset_all();

    /* 실패 N-1회 → 성공 1회 → 실패 N-1회: 연속이 아니므로 잠기지 않음 */
    for (uint32_t i = 0; i + 1U < WATER_FAIL_MAX_COUNT; i++)
    {
        now = run_cycle(now, LOW, LOW) + WATERING_TEST_GAP_MS;
    }
    now = run_cycle(now, LOW, (uint8_t)(LOW + WATER_FAIL_MIN_RISE)) + WATERING_TEST_GAP_MS;
    for (uint32_t i = 0; i + 1U < WATER_FAIL_MAX_COUNT; i++)
    {
        now = run_cycle(now, LOW, LOW) + WATERING_TEST_GAP_MS;
    }
    CHECK(watering_get_state() == WATERING_IDLE);
}

static void test_moisture_drop_counts_as_fail(void)
{
    reset_all();
    run_cycle(0, LOW, (uint8_t)(LOW - 5U));
    /* WATER_FAIL_MAX_COUNT가 1이면 바로 잠김, 아니면 IDLE */
    CHECK(watering_get_state() == (WATER_FAIL_MAX_COUNT <= 1U ? WATERING_LOCKED_FAIL : WATERING_IDLE));
}

static void test_hourly_limit(void)
{
    uint32_t now = 0;
    reset_all();

    /* 흡수 대기 때문에 1시간 안에 한도만큼 급수할 수 없으면 SR-03은 걸릴 일이 없음 */
    if ((uint64_t)CYCLE_MS * PUMP_MAX_COUNT_PER_HOUR >= 3600000U)
    {
        printf("  SKIP test_hourly_limit: (PUMP_ON_MS + SOAK_WAIT_MS) x %u >= 1시간 → SR-03 도달 불가\n",
               (unsigned)PUMP_MAX_COUNT_PER_HOUR);
        return;
    }

    /* 성공하는 급수를 한도만큼 → 1시간 안에 한 번 더 시도하면 잠김 */
    for (uint32_t i = 0; i < PUMP_MAX_COUNT_PER_HOUR; i++)
    {
        now = run_cycle(now, LOW, HIGH);
    }
    CHECK(fake_start_calls == (int)PUMP_MAX_COUNT_PER_HOUR);
    CHECK(watering_get_state() == WATERING_IDLE);

    watering_update(APP_OK, LOW, now);
    CHECK(watering_get_state() == WATERING_LOCKED_LIMIT);   /* SR-03 */
    CHECK(fake_start_calls == (int)PUMP_MAX_COUNT_PER_HOUR);

    /* 1시간이 지나도 리셋 전까지 유지 */
    watering_update(APP_OK, LOW, now + 2U * 3600000U);
    CHECK(watering_get_state() == WATERING_LOCKED_LIMIT);
}

static void test_hourly_limit_window_slides(void)
{
    uint32_t now = 0;
    reset_all();

    /* 급수 간격을 1시간 / 한도 보다 길게 → 몇 번을 해도 잠기지 않음 */
    uint32_t gap = 3600000U / PUMP_MAX_COUNT_PER_HOUR + 1000U;
    if (gap < CYCLE_MS)
    {
        gap = CYCLE_MS;
    }
    for (uint32_t i = 0; i < PUMP_MAX_COUNT_PER_HOUR * 3U; i++)
    {
        uint32_t start = now;
        run_cycle(now, LOW, HIGH);
        now = start + gap;
    }
    CHECK(watering_get_state() == WATERING_IDLE);
    CHECK(fake_start_calls == (int)(PUMP_MAX_COUNT_PER_HOUR * 3U));
}

static void test_tick_overflow(void)
{
    reset_all();
    uint32_t now = 0xFFFFFFFFU - PUMP_ON_MS;   /* 대기 도중 HAL_GetTick()이 0으로 넘어감 */

    watering_update(APP_OK, LOW, now);
    fake_pump_finished();
    now += PUMP_ON_MS;
    watering_update(APP_OK, LOW, now);
    CHECK(watering_get_state() == WATERING_SOAK_WAIT);

    watering_update(APP_OK, LOW, now + 1000U);   /* 오버플로 직후: 아직 대기 중이어야 함 */
    CHECK(watering_get_state() == WATERING_SOAK_WAIT);

    watering_update(APP_OK, HIGH, now + SOAK_WAIT_MS);
    CHECK(watering_get_state() == WATERING_IDLE);
}

static void test_pump_start_failure_not_counted(void)
{
    reset_all();
    fake_start_result = APP_ERR_NOT_READY;
    watering_update(APP_OK, LOW, 1000);
    CHECK(watering_get_state() == WATERING_IDLE);

    /* 드라이버가 정상이 되면 다음 주기에 급수, 실패한 시도는 SR-03 횟수에 안 들어감 */
    fake_start_result = APP_OK;
    uint32_t now = 2000;
    for (uint32_t i = 0; i < PUMP_MAX_COUNT_PER_HOUR; i++)
    {
        now = run_cycle(now, LOW, HIGH);
    }
    CHECK(watering_get_state() == WATERING_IDLE);
}

int main(void)
{
    printf("config: LOW_THRESHOLD=%u ON=%u SOAK=%u MAX/H=%u MIN_RISE=%u FAIL_MAX=%u\n",
           (unsigned)SOIL_LOW_THRESHOLD, (unsigned)PUMP_ON_MS, (unsigned)SOAK_WAIT_MS,
           (unsigned)PUMP_MAX_COUNT_PER_HOUR, (unsigned)WATER_FAIL_MIN_RISE,
           (unsigned)WATER_FAIL_MAX_COUNT);

    test_idle_no_water_when_moist();
    test_normal_cycle();
    test_sensor_error_in_idle();
    test_sensor_error_while_pumping();
    test_sensor_error_during_soak();
    test_fail_lock();
    test_fail_count_reset_on_success();
    test_moisture_drop_counts_as_fail();
    test_hourly_limit();
    test_hourly_limit_window_slides();
    test_tick_overflow();
    test_pump_start_failure_not_counted();

    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
