#ifndef CONFIG_H
#define CONFIG_H

/*
 * 튜닝 값은 전부 이 파일에서만 관리한다.
 * 이름은 요구사항 명세서 v1.0 기준. (TBD) 표시는 실측 전 임시값.
 * 명세서에 이름이 없는 항목은 [명세서 미정] 표시 — 회의에서 확정 후 명세서에 반영.
 */

/* ---- 실행 주기 (ms) ---- */
#define SOIL_READ_PERIOD_MS        1000U   /* FR-01 (TBD, 가정 1000ms) */
#define LIGHT_READ_PERIOD_MS       1000U   /* FR-03 [명세서 미정] */

/* ---- 토양 센서 ---- */
#define SOIL_MOVING_AVG_COUNT      12U     /* FR-01 12회 이동평균 [명세서 미정: 이름] */
#define SOIL_RAW_DRY               3000U   /* FR-02 건조(공기 중) 기준 (TBD) [명세서 미정: 이름] */
#define SOIL_RAW_WET               1300U   /* FR-02 침수(물 속) 기준 (TBD) [명세서 미정: 이름] */
#define SOIL_RAW_MARGIN            200U    /* FR-11, SR-02 이상 판정 마진 (TBD) [명세서 미정: 이름] */

/* ---- 급수 ---- */
#define SOIL_LOW_THRESHOLD         30U     /* FR-04 급수 하한 습도(%) (TBD) */
#define PUMP_ON_MS                 3000U   /* FR-04 1회 급수 시간 (TBD: 유량 실측) */
#define PUMP_MAX_ON_MS             10000U  /* FR-04 안전 상한 */
#define SOAK_WAIT_MS               60000U  /* FR-05, FR-12 흡수 대기 시간 (TBD) */
#define PUMP_MAX_COUNT_PER_HOUR    5U      /* SR-03 (TBD) */
#define WATER_FAIL_MIN_RISE        5U      /* FR-12 습도 상승 기준(%) (TBD) [명세서 미정: 이름] */
#define WATER_FAIL_MAX_COUNT       3U      /* FR-12 연속 실패 N (TBD) [명세서 미정: 이름] */

/* ---- 보광 (FR-06: ON 임계 < OFF 임계) ---- */
#define LIGHT_ON_LUX               250U    /* 이 값 미만이면 LED ON (TBD) [명세서 미정: 이름] */
#define LIGHT_OFF_LUX              350U    /* 이 값 초과면 LED OFF (TBD) [명세서 미정: 이름] */

/* ---- 통신 ---- */
#define MAX_RETRY                  3U      /* FR-08 I2C 재시도 횟수 (TBD) */

/* ---- 하드웨어 극성 ---- */
#define PUMP_RELAY_ACTIVE_LOW      1       /* SR-01 릴레이 active 레벨 확인 필요 [명세서 미정: 이름] */

#endif /* CONFIG_H */
