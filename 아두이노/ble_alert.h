// ble_alert.h — ESP32 BLE GATT 서버 (GPS + 속도 → 스마트폰)
#ifndef BLE_ALERT_H
#define BLE_ALERT_H

#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

#include "ble_ota.h"

// ── UUID ──────────────────────────────────────
#define TESLCAN_SERVICE_UUID    "0000ff01-0000-1000-8000-00805f9b34fb"
#define TESLCAN_CHAR_SPEED_UUID "0000ff03-0000-1000-8000-00805f9b34fb"
#define TESLCAN_CHAR_GPS_UUID   "0000ff04-0000-1000-8000-00805f9b34fb"

static BLEServer* pServer = nullptr;
static BLECharacteristic* pSpeedChar = nullptr;
static BLECharacteristic* pGpsChar = nullptr;
static bool bleClientConnected = false;

class BLEAlertServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* s) override {
    (void)s;
    bleClientConnected = true;
    Serial.println("[BLE] Phone app CONNECTED");
  }
  void onDisconnect(BLEServer* s) override {
    bleClientConnected = false;
    Serial.println("[BLE] Phone app disconnected");
    delay(500);
    s->startAdvertising();
    Serial.println("[BLE] Re-advertising...");
  }
};

void bleAlertInit() {
  BLEDevice::init("TeslaCAN");

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new BLEAlertServerCallbacks());

  BLEService* pService = pServer->createService(TESLCAN_SERVICE_UUID);

  // 속도
  pSpeedChar = pService->createCharacteristic(
    TESLCAN_CHAR_SPEED_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pSpeedChar->addDescriptor(new BLE2902());

  // GPS (위도, 경도, 위성수, fix)
  pGpsChar = pService->createCharacteristic(
    TESLCAN_CHAR_GPS_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pGpsChar->addDescriptor(new BLE2902());

  pService->start();

  otaSetup(pServer);

  BLEAdvertising* pAdv = BLEDevice::getAdvertising();
  pAdv->addServiceUUID(TESLCAN_SERVICE_UUID);
  pAdv->addServiceUUID(OTA_SERVICE_UUID);
  pAdv->setScanResponse(true);
  pAdv->setMinPreferred(0x06);
  pAdv->start();

  Serial.println("[BLE] GATT server started");
  Serial.println("[BLE] Advertising as 'TeslaCAN'");
}

// ── 속도 전송 (2바이트) ───────────────────────
void bleSpeedSend(uint16_t speedKmh) {
  if (!bleClientConnected || !pSpeedChar || otaIsActive()) return;
  uint8_t data[2];
  data[0] = (speedKmh >> 8) & 0xFF;
  data[1] = speedKmh & 0xFF;
  pSpeedChar->setValue(data, 2);
  pSpeedChar->notify();
}

// ── GPS 전송 (10바이트) ───────────────────────
// lat/lon을 int32 (x1,000,000), satellites, hasFix
void bleGpsSend(double lat, double lon, uint8_t satellites, bool hasFix) {
  if (!bleClientConnected || !pGpsChar || otaIsActive()) return;

  int32_t latInt = (int32_t)(lat * 1000000.0);
  int32_t lonInt = (int32_t)(lon * 1000000.0);

  uint8_t data[10];
  data[0] = (latInt >> 24) & 0xFF;
  data[1] = (latInt >> 16) & 0xFF;
  data[2] = (latInt >> 8) & 0xFF;
  data[3] = latInt & 0xFF;
  data[4] = (lonInt >> 24) & 0xFF;
  data[5] = (lonInt >> 16) & 0xFF;
  data[6] = (lonInt >> 8) & 0xFF;
  data[7] = lonInt & 0xFF;
  data[8] = satellites;
  data[9] = hasFix ? 1 : 0;

  pGpsChar->setValue(data, 10);
  pGpsChar->notify();

  Serial.printf("[BLE] GPS: %.6f, %.6f sat=%d fix=%d\n",
    lat, lon, satellites, hasFix);
}

bool bleIsConnected() {
  return bleClientConnected;
}

#endif
