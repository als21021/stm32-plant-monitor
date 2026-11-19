# CLAUDE.md

STM32F411RE Nucleo 기반 스마트 화분. 토양 습도·조도를 측정해 자동 급수와 LED 보광을 한다.
1차 범위는 로컬 제어까지 (WiFi·원격 모니터링·수위 감지·RTC 스케줄·모바일 앱·PWM 제외).

## 기준 문서 (Notion)

설계 판단의 근거는 Notion이 원본이다. 코드와 문서가 어긋나면 임의로 정하지 말고 확인할 것.

- 요구사항 명세서 (v1.3, 구현 기준선 v1.0 이후): https://app.notion.com/p/3e6c8ac7029e81ff9a90d76b577ed684
- 로드맵 및 역할 분담: https://app.notion.com/p/3d9c8ac7029e81b7832ce98e4f1fdc50
- 팀 운영 규칙 (커밋·네이밍): https://app.notion.com/p/3d9c8ac7029e81769553f96af8ee9a1e
- 회의록 DB: 대시보드 → Meeting Notes

코드 주석의 `FR-xx`, `SR-xx`, `NFR-xx`는 명세서 요구사항 ID다.

## 환경

- STM32CubeIDE 2.2.0, C, HAL 우선 (성능 이슈 시에만 LL) — NFR-01
- 빌드는 CubeIDE에서 한다 (`Debug/`는 생성물)
- `.ioc`(CubeMX 설정)는 김민산만 수정. 재생성은 다른 작업과 섞지 않고 `chore` 커밋으로 단독 병합
- 직접 작성한 코드는 모듈 파일이나 CubeMX의 `USER CODE BEGIN/END` 영역 안에만 둔다

## 구조 (NFR-03)

| 역할 | 파일 | 담당 |
|---|---|---|
| 센서 | `soil_sensor.c` (ADC, PA0), `light_sensor.c` (BH1750, I2C) | 민산 / 상진 |
| 판단 로직 | `watering.c`, `lighting.c` | 민산 / 상진 |
| 액추에이터 | `pump.c` (릴레이, PA8 오픈 드레인), `led.c` | 민산 / 상진 |
| 공통 | `config.h` (튜닝 상수 전부), `app_status.h` (공통 반환값) | 공동 |

- `main.c`에는 초기화와 모듈 함수 호출만 둔다. HAL 주변장치 호출(ADC·I2C·GPIO)은 해당 드라이버 모듈 안에서만.
- 판단 로직(`watering.c`, `lighting.c`)은 HAL을 직접 부르지 않고 드라이버 함수만 사용한다.
- 튜닝 값은 `config.h`에만 둔다. `(TBD)` 표시는 실측 전 임시값.

## 코딩 규칙

- 논블로킹 (NFR-04): 제어 로직에서 `HAL_Delay`로 긴 블로킹 금지. `HAL_GetTick()` 기반, 경과 비교는 항상 `now - start >= period` 형태 (tick 오버플로 대응). `now_ms`는 호출자가 넘긴다.
- 네이밍: 함수·변수·파일 `snake_case`, 모듈 접두어 + 동사 (`soil_sensor_read`, `pump_start`), 매크로 `UPPER_CASE`, 타입 `_t` 접미사
- 반환값은 `app_status_t` 사용
- 주석은 한글, 관련 요구사항 ID를 함께 적는다

## 안전 요구사항 (구현 시 항상 확인)

- SR-01: 전원/리셋 직후 펌프·LED OFF (`PUMP_RELAY_ACTIVE_LOW` 극성 주의)
- SR-02 / FR-11: 토양 센서 raw가 `SOIL_RAW_WET - MARGIN ~ SOIL_RAW_DRY + MARGIN` 밖이면 펌프 금지, 복귀 시 해제. 조도·SD 에러는 급수를 막지 않는다
- SR-03: 시간당 `PUMP_MAX_COUNT_PER_HOUR` 초과 시 리셋 전까지 급수 중단
- FR-04: 1회 급수 시간(`PUMP_ON_MS`) 방식, `PUMP_MAX_ON_MS`는 절대 상한
- FR-05: 펌프 OFF 후 `SOAK_WAIT_MS` 동안 재급수 판정 중단
- FR-12: 흡수 대기 후 습도 상승 < `WATER_FAIL_MIN_RISE`이면 실패, 연속 `WATER_FAIL_MAX_COUNT`회면 급수 중단
- FR-13: SD카드 실패를 `Error_Handler()`로 보내지 않는다. 제어는 계속
- SR-05: IWDG 갱신은 main 루프 끝 한 곳에서만 (안정화 단계에서 활성화)

## Git

- 커밋: `<타입>: <한 줄 요약>` — 타입 `feat|fix|docs|refactor|chore`, 한글, 50자 이내, 명사형 종결 ("~추가", "~수정"). 한 커밋에 한 작업. `fix`는 본문에 원인 한 줄
- 브랜치는 기능별(`feat/...`, `chore/...`, `docs/...`)로 만들고 PR → 리뷰 승인 → Squash and merge → 브랜치 삭제
- Squash merge라서 머지된 로컬 브랜치는 `git branch -D`로 지운다 (내용이 main에 들어갔는지 먼저 확인)
- 미정 (5주차 회의 안건): PR 전 `origin/main` 머지 규칙, 브랜치 접두어 `feat/` vs `feature/`

## 테스트

- 판단 로직은 PC에서 단위 테스트한다: `make -C test` (가짜 드라이버 + 시간 주입, `test/test_watering.c`)
- `test/`는 CubeIDE 소스 경로(`Core`, `Drivers`)에 포함되지 않아 펌웨어 빌드에 영향 없음
- 보드 확인용 임시 코드(`main.c`)는 커밋하지 않는다

## 일정 (로드맵)

- ~10월: 모듈 개발 (super loop)
- 11/2~11/13: 로직 통합 (main 루프)
- 11/16~11/27: SD카드 로깅 (FatFs, SPI2)
- 12/21~1/1: FreeRTOS 전환 (NFR-02)
- 1/4~1/15: 안정화 (워치독, 장기 테스트)

## 미반영 결정 / 미확정 항목 (2026-10-14 회의 기준)

- `WATER_FAIL_MAX_COUNT` 3 → 2 로 변경 결정됨 (코드 미반영)
- `SOAK_WAIT_MS` 단위 확인 필요: 회의록 "20m"이 20분(1200000)인지, 현재 60000
- IWDG 연속 리셋 횟수 카운터는 `.noinit` RAM에 저장하기로 결정 (상수 이름·값 미정)
- 실측 대기: 펌프 유량 → `PUMP_ON_MS`, 급수 임계값, `SOAK_WAIT_MS`, 조도 임계값, `MAX_RETRY`, `SOIL_RAW_MARGIN`
- `SOAK_WAIT_MS`가 20분이면 1시간 최대 3회 급수라 `PUMP_MAX_COUNT_PER_HOUR`(5)에 도달 불가 → SR-03 한도 재검토 필요
- 펌프 실제 연결은 5V 전원(USB 충전기·보조배터리 + USB-A to 2wire 케이블) 준비 후. 그 전까지 `PUMP_ON_MS`는 임시값
