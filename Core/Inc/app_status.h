#ifndef APP_STATUS_H
#define APP_STATUS_H

/* 모든 모듈 공통 반환 상태 */
typedef enum {
    APP_OK = 0,
    APP_ERR_NOT_READY,   /* 초기화 전 또는 아직 측정 전 */
    APP_ERR_TIMEOUT,     /* 변환·통신 시간 초과 */
    APP_ERR_RANGE,       /* 값이 정상 범위 밖 (SR-02, FR-11) */
    APP_ERR_COMM         /* I2C/SPI 통신 실패 */
} app_status_t;

#endif /* APP_STATUS_H */
