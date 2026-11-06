#include "pump.h"      /* 이 모듈이 공개하는 함수 선언 */
#include "main.h"      /* HAL 함수, PUMP_RELAY_Pin / PUMP_RELAY_GPIO_Port 정의 */
#include "config.h"    /* PUMP_MAX_ON_MS, PUMP_RELAY_ACTIVE_LOW */

/*
 * 하드웨어: PA8(PUMP_RELAY) 오픈 드레인 출력 + 외부 5V 풀업 → S8550 PNP Low 트리거 릴레이
 * - RESET(Low)  : 트랜지스터가 핀을 GND로 당김 → 릴레이 ON
 * - SET(High-Z) : 핀을 놓아줌 → 풀업이 5V로 끌어올림 → 릴레이 OFF
 * 오픈 드레인에서 SET은 "3.3V 출력"이 아니라 "출력 안 함"이라는 점이 핵심
 */
#if PUMP_RELAY_ACTIVE_LOW
#define PUMP_LEVEL_ON   GPIO_PIN_RESET
#define PUMP_LEVEL_OFF  GPIO_PIN_SET
#else
#define PUMP_LEVEL_ON   GPIO_PIN_SET
#define PUMP_LEVEL_OFF  GPIO_PIN_RESET
#endif

/* static 전역: 이 파일 안에서만 보이는 펌프 상태 */
static bool     is_initialized = false;
static bool     is_running     = false;
static uint32_t start_ms       = 0;   /* 펌프를 켠 시각 (HAL_GetTick 기준) */
static uint32_t run_ms         = 0;   /* 이번에 켜 둘 시간 (상한 적용 후) */

/* 핀 출력은 이 두 함수에서만 → 극성 실수가 한 곳에만 생기게 함 */
static void relay_on(void)
{
    HAL_GPIO_WritePin(PUMP_RELAY_GPIO_Port, PUMP_RELAY_Pin, PUMP_LEVEL_ON);
}

static void relay_off(void)
{
    HAL_GPIO_WritePin(PUMP_RELAY_GPIO_Port, PUMP_RELAY_Pin, PUMP_LEVEL_OFF);
}

/*
 * SR-01: 리셋 직후 펌프 OFF 보장
 * CubeMX 초기 레벨(PinState=SET)로 MX_GPIO_Init()에서 이미 OFF지만,
 * .ioc가 바뀌어도 안전하도록 여기서 한 번 더 명시적으로 끈다
 */
app_status_t pump_init(void)
{
    relay_off();
    is_running = false;
    start_ms = 0;
    run_ms = 0;
    is_initialized = true;
    return APP_OK;
}

/*
 * 반환값:
 * - APP_OK            : 펌프 ON 시작
 * - APP_ERR_NOT_READY : init 전이거나 이미 동작 중 (기존 타이머는 그대로 유지)
 * - APP_ERR_RANGE     : duration_ms == 0 (켤 시간이 없음)
 */
app_status_t pump_start(uint32_t duration_ms, uint32_t now_ms)
{
    if (!is_initialized || is_running)
    {
        return APP_ERR_NOT_READY;
    }

    if (duration_ms == 0U)
    {
        return APP_ERR_RANGE;
    }

    /* FR-04 안전 상한: 호출자가 큰 값을 넘겨도 PUMP_MAX_ON_MS를 넘기지 않음 */
    if (duration_ms > PUMP_MAX_ON_MS)
    {
        duration_ms = PUMP_MAX_ON_MS;
    }

    /* 상태를 먼저 기록하고 출력 → 출력 직후 무슨 일이 생겨도 update가 끌 수 있음 */
    start_ms = now_ms;
    run_ms = duration_ms;
    is_running = true;
    relay_on();
    return APP_OK;
}

void pump_stop(void)
{
    /* 상태와 관계없이 항상 OFF 출력 → 어떤 상황에서 불러도 안전한 쪽으로 */
    relay_off();
    is_running = false;
}

/*
 * main 루프에서 매 회 호출
 * (now_ms - start_ms)는 unsigned 뺄셈이라 HAL_GetTick()이
 * 0xFFFFFFFF → 0으로 넘어가도(약 49.7일) 경과 시간이 올바르게 나옴
 * 예) start = 0xFFFFFF00, now = 0x00000100 → 차이 = 0x200 (512ms)
 * 반대로 "now >= start + run" 형태로 비교하면 넘어갈 때 오동작함
 */
void pump_update(uint32_t now_ms)
{
    if (!is_running)
    {
        return;
    }

    if ((uint32_t)(now_ms - start_ms) >= run_ms)
    {
        pump_stop();
    }
}

bool pump_is_running(void)
{
    return is_running;
}
