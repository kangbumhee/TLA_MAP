// bt_audio.h — TeslaCAN Bluetooth A2DP Source (Test 1 Fix)
// Tesla가 ESP32를 찾아서 연결하는 방식 — ESP32는 스캔/타깃 연결을 하지 않고 대기
#ifndef BT_AUDIO_H
#define BT_AUDIO_H

#include <Arduino.h>
#include <BluetoothA2DPSource.h>
#include "esp_idf_version.h"
#include <math.h>

#ifndef BT_DEVICE_NAME
#define BT_DEVICE_NAME "TeslaCAN"
#endif

#ifndef SAMPLE_RATE
#define SAMPLE_RATE 44100
#endif

#ifndef BEEP_VOLUME
#define BEEP_VOLUME 0.6f
#endif

#ifndef BT_PI
#define BT_PI 3.14159265358979323846f
#endif

// ── 경고 패턴 ─────────────────────────────────
struct BeepPattern {
    float freqHz;
    int onMs;
    int offMs;
    int repeatCount; // -1 = 무한반복
};

static const BeepPattern PATTERN_FAR = {800.0f, 150, 850, 3};
static const BeepPattern PATTERN_MID = {1000.0f, 150, 350, 5};
static const BeepPattern PATTERN_NEAR = {1200.0f, 100, 100, 10};
static const BeepPattern PATTERN_OVER = {1500.0f, 80, 80, -1};
static const BeepPattern PATTERN_NONE = {0, 0, 0, 0};

// ── 상태 ──────────────────────────────────────
static BluetoothA2DPSource a2dp_source;
static volatile bool btConnected = false;

// beep 상태
static volatile bool beepActive = false;
static BeepPattern activePattern = PATTERN_NONE;
static float phaseAccum = 0.0f;
static bool beepOn = false;
static uint64_t beepSampleIndex = 0;

// ----- 호환용 Alert API (기존 .ino 코드 유지) -----
enum AlertLevel { ALERT_NONE = 0, ALERT_FAR, ALERT_MID, ALERT_NEAR, ALERT_DANGER };

AlertLevel distanceToLevel(int distM, int speed, int speedLimit) {
    if (speedLimit > 0 && speed > speedLimit + 10) return ALERT_DANGER;
    if (distM < 300) return ALERT_NEAR;
    if (distM < 600) return ALERT_MID;
    if (distM < 1000) return ALERT_FAR;
    return ALERT_NONE;
}

void btAudioAlert(const BeepPattern &pattern);
void btAudioStop();

void btAudioAlert(AlertLevel level, int speedLimit, int distanceM) {
    (void)speedLimit;
    (void)distanceM;
    if (level == ALERT_NONE) {
        btAudioStop();
        return;
    }

    switch (level) {
        case ALERT_FAR: btAudioAlert(PATTERN_FAR); break;
        case ALERT_MID: btAudioAlert(PATTERN_MID); break;
        case ALERT_NEAR: btAudioAlert(PATTERN_NEAR); break;
        case ALERT_DANGER: btAudioAlert(PATTERN_OVER); break;
        default: btAudioAlert(PATTERN_FAR); break;
    }
}

// ── 연결 상태 콜백 ────────────────────────────
void bt_connection_state_cb(esp_a2d_connection_state_t state, void *obj) {
    (void)obj;
    switch (state) {
        case ESP_A2D_CONNECTION_STATE_CONNECTED:
            btConnected = true;
            Serial.println("[BT] === CONNECTED! ===");
            break;
        case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
            btConnected = false;
            Serial.println("[BT] Disconnected");
            break;
        case ESP_A2D_CONNECTION_STATE_CONNECTING:
            Serial.println("[BT] Connecting...");
            break;
        case ESP_A2D_CONNECTION_STATE_DISCONNECTING:
            Serial.println("[BT] Disconnecting...");
            break;
        default: break;
    }
}

// ── 오디오 콜백 ────────────────────────────────
int32_t bt_audio_callback(Frame *frames, int32_t frameCount) {
    if (!beepActive || activePattern.freqHz == 0.0f) {
        for (int32_t i = 0; i < frameCount; i++) {
            frames[i].channel1 = 0;
            frames[i].channel2 = 0;
        }
        return frameCount;
    }

    const float phaseInc = (2.0f * BT_PI * activePattern.freqHz) / static_cast<float>(SAMPLE_RATE);

    for (int32_t i = 0; i < frameCount; i++) {
        const int cycleMs = activePattern.onMs + activePattern.offMs;

        if (cycleMs > 0) {
            // 샘플 단위 시간으로 ON/OFF를 계산 (버퍼 내에서도 정확히 진행)
            const uint64_t elapsedMs =
                (beepSampleIndex * 1000ULL) / static_cast<uint64_t>(SAMPLE_RATE);
            const int cycleIndex = static_cast<int>(elapsedMs / static_cast<uint64_t>(cycleMs));
            const int posInCycle = static_cast<int>(elapsedMs % static_cast<uint64_t>(cycleMs));
            beepOn = (posInCycle < activePattern.onMs);

            if (activePattern.repeatCount >= 0 && cycleIndex >= activePattern.repeatCount) {
                beepActive = false;
                frames[i].channel1 = 0;
                frames[i].channel2 = 0;
                beepSampleIndex++;
                continue;
            }
        }

        if (beepOn) {
            const float sample = sinf(phaseAccum) * BEEP_VOLUME * 32767.0f;
            const int16_t s = static_cast<int16_t>(sample);
            frames[i].channel1 = s;
            frames[i].channel2 = s;
            phaseAccum += phaseInc;
            if (phaseAccum > 2.0f * BT_PI) phaseAccum -= 2.0f * BT_PI;
        } else {
            frames[i].channel1 = 0;
            frames[i].channel2 = 0;
        }

        beepSampleIndex++;
    }

    return frameCount;
}

// ── 경고음 트리거 ──────────────────────────────
void btAudioAlert(const BeepPattern &pattern) {
    if (!a2dp_source.is_connected() && !btConnected) {
        Serial.println("[BT] Not connected, skip alert");
        return;
    }

    activePattern = pattern;
    phaseAccum = 0;
    beepSampleIndex = 0;
    beepOn = true;
    beepActive = true;

    Serial.printf("[BT] Alert: %.0f Hz, on=%d off=%d repeat=%d\n", static_cast<double>(pattern.freqHz),
                  pattern.onMs, pattern.offMs, pattern.repeatCount);
}

void btAudioStop() {
    beepActive = false;
    activePattern = PATTERN_NONE;
    beepSampleIndex = 0;
    Serial.println("[BT] Alert stopped");
}

// ── 초기화 ─────────────────────────────────────
static bool btAudioStarted = false;

void btAudioInit() {
    if (btAudioStarted) return;

    Serial.println("[BT] A2DP Source starting (waiting for Tesla)...");

    // 1) 로컬 이름 설정 — Tesla에서 이 이름으로 보임
    a2dp_source.set_local_name(BT_DEVICE_NAME);

    // 2) 오디오 콜백 등록
    a2dp_source.set_data_callback_in_frames(bt_audio_callback);

    // 3) 연결 상태 콜백
    a2dp_source.set_on_connection_state_changed(bt_connection_state_cb, nullptr);

    // 4) 자동 재연결 활성화 (한번 페어링 후 자동 연결)
    a2dp_source.set_auto_reconnect(true);

    // 5) 볼륨 설정
    a2dp_source.set_volume(100);

    // 6) SSP (Secure Simple Pairing) 활성화 — Tesla와 페어링 시 필요
    a2dp_source.set_ssp_enabled(true);

    // 7) discoverable/connectable — Tesla가 찾을 수 있도록 (start 이전에 설정)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
    a2dp_source.set_discoverability(ESP_BT_GENERAL_DISCOVERABLE);
#endif
    a2dp_source.set_connectable(true);

    // 8) 핵심: start() 무인자 → 스캔/타깃 연결 없이 스택만 올리고 incoming 연결 대기
    a2dp_source.start();

    Serial.println("[BT] A2DP Source started (waiting for Tesla)");
    Serial.printf("[BT] Device name: \"%s\"\n", BT_DEVICE_NAME);
    Serial.println("[BT] >>> Tesla Bluetooth > 'Add New Device' > \"TeslaCAN\"");

    btAudioStarted = true;
}

bool btAudioConnected() { return a2dp_source.is_connected(); }

// 호환용 별칭
bool btIsConnected() { return btAudioConnected(); }

#endif // BT_AUDIO_H
