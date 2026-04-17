// ble_ota.h — BLE OTA 펌웨어 업데이트
#ifndef BLE_OTA_H
#define BLE_OTA_H

#include <Arduino.h>
#include <Update.h>
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

#define OTA_SERVICE_UUID "fb1e4002-54ae-4a28-9f74-dfccb248601d"
#define OTA_CTRL_UUID    "fb1e4004-54ae-4a28-9f74-dfccb248601d"
#define OTA_DATA_UUID    "fb1e4003-54ae-4a28-9f74-dfccb248601d"

#define FW_VERSION "1.0.0"

static bool otaActive = false;
static size_t otaReceived = 0;
static size_t otaTotal = 0;
static unsigned long otaStartMs = 0;
static int otaLastPct = -1;

// ── 제어 특성 콜백 ───────────────────────────
class OtaCtrlCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pChar) override {
        String val = String(pChar->getValue().c_str());

        if (val.startsWith("START:")) {
            otaTotal = static_cast<size_t>(val.substring(6).toInt());
            otaReceived = 0;
            otaLastPct = -1;

            if (otaTotal == 0 || otaTotal > 2000000) {
                Serial.printf("[OTA] 크기 오류: %u\n", static_cast<unsigned>(otaTotal));
                pChar->setValue("ERR:SIZE");
                pChar->notify();
                return;
            }

            if (!Update.begin(otaTotal)) {
                Serial.println("[OTA] Update.begin() 실패");
                Serial.println(Update.errorString());
                pChar->setValue("ERR:BEGIN");
                pChar->notify();
                return;
            }

            otaActive = true;
            otaStartMs = millis();
            Serial.println("==========================================");
            Serial.printf("[OTA] 시작: %u bytes\n", static_cast<unsigned>(otaTotal));
            Serial.println("==========================================");
            pChar->setValue("OK");
            pChar->notify();
        } else if (val == "END") {
            if (!otaActive) {
                pChar->setValue("ERR:NOT_ACTIVE");
                pChar->notify();
                return;
            }

            if (Update.end(true)) {
                unsigned long elapsed = (millis() - otaStartMs) / 1000;
                Serial.println("==========================================");
                Serial.printf("[OTA] 완료! %u bytes, %lu초\n",
                              static_cast<unsigned>(otaReceived), elapsed);
                Serial.println("[OTA] 3초 후 재부팅...");
                Serial.println("==========================================");
                pChar->setValue("OK:REBOOT");
                pChar->notify();
                delay(3000);
                ESP.restart();
            } else {
                Serial.print("[OTA] 종료 실패: ");
                Serial.println(Update.errorString());
                pChar->setValue("ERR:END");
                pChar->notify();
                otaActive = false;
            }
        } else if (val == "CANCEL") {
            if (otaActive) {
                Update.abort();
                otaActive = false;
                otaReceived = 0;
                Serial.println("[OTA] 취소됨");
            }
            pChar->setValue("OK:CANCEL");
            pChar->notify();
        } else if (val == "VERSION") {
            pChar->setValue(FW_VERSION);
            pChar->notify();
            Serial.printf("[OTA] 버전 요청 -> %s\n", FW_VERSION);
        }
    }
};

// ── 데이터 특성 콜백 ──────────────────────────
class OtaDataCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pChar) override {
        if (!otaActive) return;

        uint8_t* data = pChar->getData();
        size_t len = pChar->getLength();
        if (len == 0) return;

        size_t written = Update.write(data, len);
        otaReceived += written;

        if (written != len) {
            Serial.printf("[OTA] 쓰기 오류: %u / %u\n",
                          static_cast<unsigned>(written), static_cast<unsigned>(len));
            Serial.println(Update.errorString());
            Update.abort();
            otaActive = false;
            return;
        }

        int pct = (otaTotal > 0) ? static_cast<int>(otaReceived * 100 / otaTotal) : 0;
        if (pct / 5 != otaLastPct / 5) {
            Serial.printf("[OTA] %3d%%  (%u / %u)\n", pct,
                          static_cast<unsigned>(otaReceived), static_cast<unsigned>(otaTotal));
            otaLastPct = pct;
        }
    }
};

// ── 전역 ──────────────────────────────────────
static BLEService* otaService = nullptr;
static BLECharacteristic* otaCtrlChar = nullptr;
static BLECharacteristic* otaDataChar = nullptr;

// ── 초기화 ────────────────────────────────────
void otaSetup(BLEServer* pServer) {
    otaService = pServer->createService(OTA_SERVICE_UUID);

    otaCtrlChar = otaService->createCharacteristic(
        OTA_CTRL_UUID,
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY |
        BLECharacteristic::PROPERTY_READ);
    otaCtrlChar->setCallbacks(new OtaCtrlCallbacks());
    otaCtrlChar->addDescriptor(new BLE2902());
    otaCtrlChar->setValue(FW_VERSION);

    otaDataChar = otaService->createCharacteristic(
        OTA_DATA_UUID,
        BLECharacteristic::PROPERTY_WRITE);
    otaDataChar->setCallbacks(new OtaDataCallbacks());

    otaService->start();
    Serial.printf("[OTA] BLE OTA 서비스 준비 (v%s)\n", FW_VERSION);
}

inline bool otaIsActive() {
    return otaActive;
}

#endif
