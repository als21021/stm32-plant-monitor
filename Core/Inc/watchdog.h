/*
 * watchdog.h
 *
 * IWDG(독립 워치독) 관리 + 연속 IWDG 리셋 횟수 기록
 * - IWDG 설정값(프리스케일러, reload)은 CubeMX(iwdg.c)에서 관리
 *   현재: LSI 32kHz / 64 = 500Hz, reload 999 → 약 2초 안에 kick 안 하면 리셋
 * - 리셋 횟수는 .noinit RAM에 저장 (회의 결정 A안)
 *   시스템 리셋(IWDG, NRST 버튼)에는 값이 유지되고, 전원 차단 시 초기화됨
 * - 튜닝 값(IWDG_RESET_LIMIT, IWDG_RESET_CLEAR_MS)은 config.h에서 관리
 */

#ifndef INC_WATCHDOG_H_
#define INC_WATCHDOG_H_

#include <stdint.h>
#include <stdbool.h>
#include "main.h"
#include "config.h"

/**
 * @brief  리셋 원인 확인 + 연속 IWDG 리셋 횟수 갱신
 * @note   main()에서 주변장치 초기화 직후, watchdog_start()보다 먼저 한 번만 호출.
 *         RCC 리셋 플래그는 확인 후 지움 (다음 리셋 원인을 정확히 알기 위해)
 */
void watchdog_check_reset_cause(void);

/**
 * @brief  이번 부팅이 IWDG 리셋으로 인한 것인지
 * @retval true: IWDG 리셋으로 재시작됨
 */
bool watchdog_was_iwdg_reset(void);

/**
 * @brief  연속 IWDG 리셋 횟수
 */
uint32_t watchdog_get_reset_count(void);

/**
 * @brief  연속 IWDG 리셋 횟수가 IWDG_RESET_LIMIT 이상인지
 * @note   true일 때의 동작(안전 모드)은 미정 (팀 논의 필요)
 */
bool watchdog_is_reset_limit_reached(void);

/**
 * @brief  IWDG 시작 (한 번 시작하면 리셋 전까지 멈출 수 없음)
 * @note   디버거로 정지(breakpoint)했을 때는 IWDG도 멈추도록 설정함
 */
void watchdog_start(void);

/**
 * @brief  IWDG 카운터 리프레시 (while 루프에서 매번 호출)
 */
void watchdog_kick(void);

/**
 * @brief  부팅 후 IWDG_RESET_CLEAR_MS 동안 정상 동작하면 리셋 횟수를 0으로 초기화
 * @note   while 루프에서 매번 호출 (논블로킹)
 */
void watchdog_check_stable_time(void);

#endif /* INC_WATCHDOG_H_ */
