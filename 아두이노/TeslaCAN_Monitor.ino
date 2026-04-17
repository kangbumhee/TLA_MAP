// ============================================================
//  TeslaCAN Monitor v3.0
//  - Bluetooth A2DP 음성 경고 (Tesla 스피커)
//  - CAN Bus 모니터링
//  - GPS 기반 과속카메라 경고
//  - Wi-Fi 제거
// ============================================================

#include <Arduino.h>

#include "sketch_config.h"
#include "can_frame_types.h"
#include "drivers/twai_driver.h"
#include "gps_handler.h"
#include "tesla_decoder.h"
#include "ble_alert.h"
#include "ble_test.h"

static TWAIDriver canDriver(TWAI_TX_PIN, TWAI_RX_PIN);

static uint32_t lastStatusPrint = 0;

static int vehicleSpeedKmh() {
    if (carData.vehicleSpeed > 1.0f) return static_cast<int>(carData.vehicleSpeed + 0.5f);
    if (gpsData.valid) return static_cast<int>(gpsData.speedKmh + 0.5f);
    return 0;
}

void setup() {
    delay(1000);
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {}

    Serial.println("================================");
    Serial.println("  TeslaCAN Monitor v3.0");
    Serial.println("  LILYGO T-CAN485 v1.1");
    Serial.println("  BLE + CAN + GPS");
    Serial.println("================================");

    // -- T-CAN485 하드웨어 초기화 --
    pinMode(ME2107_EN, OUTPUT);
    digitalWrite(ME2107_EN, HIGH);

    pinMode(CAN_SE_PIN, OUTPUT);
    digitalWrite(CAN_SE_PIN, LOW);

    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, HIGH);

    // -- CAN 시작 (Listen-Only) --
    if (canDriver.init()) {
        Serial.println("[CAN] TWAI started @ 500kbps (LISTEN_ONLY)");
    } else {
        Serial.println("[CAN] TWAI init FAILED!");
    }

    // -- GPS 시작 --
    Serial2.begin(9600, SERIAL_8N1, 34, 12);
    Serial.println("[GPS] Serial2 started");

    // -- BLE GATT server --
    bleAlertInit();

    Serial.println("================================");
    Serial.println("  Setup complete!");
    Serial.printf("  Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.println("  BLE app auto-connect ready");
    Serial.println("================================");

    testInit();
}

void loop() {
    if (otaIsActive()) {
        delay(10);
        testLoop();
        return;
    }

    uint32_t now = millis();

    // -- CAN 프레임 수신 & 디코딩 --
    CanFrame frame;
    int processedFrames = 0;
    while (processedFrames < 300 && canDriver.read(frame)) {
        digitalWrite(PIN_LED, LOW);
        decodeTeslaCAN(frame);
        processedFrames++;
    }
    digitalWrite(PIN_LED, HIGH);

    // -- GPS 파싱 --
    while (Serial2.available()) {
        gpsParser.feed(static_cast<char>(Serial2.read()));
    }

    // GPS 원시 상태 (10초마다)
    static unsigned long lastGpsRaw = 0;
    if (millis() - lastGpsRaw > 10000) {
        Serial.printf("[GPS RAW] valid=%d sat=%d lat=%.6f lng=%.6f spd=%.1f\n",
                      gpsData.valid, gpsData.satellites,
                      gpsData.lat, gpsData.lng, gpsData.speedKmh);

        // NMEA 원시 데이터 10개 출력
        Serial.print("[NMEA] ");
        unsigned long start = millis();
        while (millis() - start < 1200) {
            if (Serial2.available()) {
                char c = Serial2.read();
                Serial.print(c);
                gpsParser.feed(c);  // 파서에도 전달
            }
        }
        Serial.println();

        lastGpsRaw = millis();
    }

    // -- 속도 + GPS 전송 (1초마다, 시뮬 중에는 ble_test가 전송) --
    static unsigned long lastSendTime = 0;
    if (millis() - lastSendTime > 1000) {
        lastSendTime = millis();
        if (!testIsBleOverrideActive()) {
            bleSpeedSend(static_cast<uint16_t>(vehicleSpeedKmh()));
            bleGpsSend(gpsData.valid ? gpsData.lat : 0.0,
                       gpsData.valid ? gpsData.lng : 0.0,
                       static_cast<uint8_t>(gpsData.satellites),
                       gpsData.valid);
        }
    }

    // -- 상태 출력 (5초마다) --
    if (now - lastStatusPrint >= 5000) {
        lastStatusPrint = now;
        const int spd = vehicleSpeedKmh();
        Serial.printf("[%lus] Frames:%lu SPD:%d GPS:%s SAT:%d LAT:%.6f LNG:%.6f BLE:%s\n",
                      now / 1000, static_cast<unsigned long>(carData.frameCount), spd,
                      gpsData.valid ? "OK" : "NO", gpsData.satellites,
                      gpsData.lat, gpsData.lng,
                      bleIsConnected() ? "PHONE" : "OFF");
    }

    testLoop();

    yield();
    delay(1);
}
