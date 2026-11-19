/*
 * watchdog.c
 *
 * IWDG(독립 워치독) 관리 + 연속 IWDG 리셋 횟수 기록
 * CubeMX에서 MX_IWDG_Init 자동 호출을 꺼두었으므로 watchdog_start()에서 직접 호출함
 */

#include "watchdog.h"
#include "iwdg.h"   // hiwdg 핸들, MX_IWDG_Init()

/* .noinit 영역 값이 유효한지 표시 (전원 인가 직후엔 쓰레기값이라 구분 필요) */
#define WDG_NOINIT_MAGIC        0xA5A5C0DEU

typedef struct {
    uint32_t magic;         // WDG_NOINIT_MAGIC이면 reset_count가 유효함
    uint32_t reset_count;   // 연속 IWDG 리셋 횟수
} wdg_noinit_data_t;

/* startup 코드가 0으로 지우지 않는 영역 (링커 스크립트의 .noinit 섹션) */
__attribute__((section(".noinit"))) static wdg_noinit_data_t wdg_noinit_data;

/* 이 파일 안에서만 사용 */
static bool     wdg_iwdg_reset   = false;   // 이번 부팅이 IWDG 리셋인지
static bool     wdg_count_stable = false;   // 1시간 정상 동작 확인이 끝났는지
static uint32_t wdg_start_tick   = 0;       // 정상 동작 시간 측정 기준 시각 (ms)

void watchdog_check_reset_cause(void)
{
    /* 리셋 원인 확인 (RCC_CSR 플래그) */
    bool por_reset = (__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST) != RESET);
    wdg_iwdg_reset = (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET);

    if (por_reset || (wdg_noinit_data.magic != WDG_NOINIT_MAGIC))
    {
        /* 전원을 새로 켰거나 저장값이 유효하지 않으면 0부터 시작 */
        wdg_noinit_data.magic = WDG_NOINIT_MAGIC;
        wdg_noinit_data.reset_count = 0U;
    }
    else if (wdg_iwdg_reset)
    {
        /* IWDG 리셋이면 +1 (최댓값에서 더 올라가지 않도록 막음) */
        if (wdg_noinit_data.reset_count < UINT32_MAX)
        {
            wdg_noinit_data.reset_count++;
        }
    }
    else
    {
        /* NRST 버튼, 소프트웨어 리셋 등 IWDG 외 리셋이면 연속이 끊긴 것으로 봄 */
        wdg_noinit_data.reset_count = 0U;
    }

    /* 플래그를 지워야 다음 리셋 때 원인을 정확히 알 수 있음 */
    __HAL_RCC_CLEAR_RESET_FLAGS();

    /* 정상 동작 시간 측정 시작 (카운트가 0이면 확인할 필요 없음) */
    wdg_start_tick   = HAL_GetTick();
    wdg_count_stable = (wdg_noinit_data.reset_count == 0U);
}

bool watchdog_was_iwdg_reset(void)
{
    return wdg_iwdg_reset;
}

uint32_t watchdog_get_reset_count(void)
{
    return wdg_noinit_data.reset_count;
}

bool watchdog_is_reset_limit_reached(void)
{
    return (wdg_noinit_data.reset_count >= IWDG_RESET_LIMIT);
}

void watchdog_start(void)
{
    /* 디버거로 멈춰 있을 때 IWDG도 같이 멈춤 (디버깅 중 리셋 방지) */
    __HAL_DBGMCU_FREEZE_IWDG();

    MX_IWDG_Init();     // 이 순간부터 IWDG 동작 시작
}

void watchdog_kick(void)
{
    HAL_IWDG_Refresh(&hiwdg);
}

void watchdog_check_stable_time(void)
{
    if (wdg_count_stable)
    {
        return;
    }

    /* 부호 없는 뺄셈이라 약 49.7일 뒤 tick이 한 바퀴 돌아도 정상 동작함 */
    if ((HAL_GetTick() - wdg_start_tick) >= IWDG_RESET_CLEAR_MS)
    {
        wdg_noinit_data.reset_count = 0U;
        wdg_count_stable = true;
    }
}
