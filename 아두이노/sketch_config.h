#pragma once
// ============================================================
// TeslaCAN Monitor v2 - 설정 파일
// LILYGO T-CAN485 v1.1
// ============================================================

#define DRIVER_TWAI

// -- T-CAN485 v1.1 핀 정의 --
#define TWAI_TX_PIN   GPIO_NUM_27
#define TWAI_RX_PIN   GPIO_NUM_26
#define PIN_LED       4
#define CAN_SE_PIN    23
#define ME2107_EN     16

// -- GPS (Serial2) --
#define GPS_RX_PIN    34
#define GPS_TX_PIN    12
#define GPS_BAUD      9600

// -- SD 카드 (HSPI) --
#define SD_MOSI       15
#define SD_MISO       2
#define SD_SCLK       14
#define SD_CS         13

// -- Wi-Fi AP --
#define AP_SSID       "TeslaCAN"
#define AP_PASS       "tesla1234"

// -- 과속카메라 경고 거리 (m) --
#define CAM_WARN_FAR     600
#define CAM_WARN_NEAR    200
#define CAM_WARN_PASS    50
#define CAM_HEADING_TOL  60
