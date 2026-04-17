#ifndef BUZZER_AUDIO_H
#define BUZZER_AUDIO_H

#include <Arduino.h>

// T-CAN485에서 비교적 안전한 여유 GPIO
#ifndef BUZZER_PIN
#define BUZZER_PIN 4
#endif

struct BeepPattern {
    int freqHz;
    int onMs;
    int offMs;
    int repeatCount; // -1 = 무한 반복
};

static const BeepPattern PATTERN_FAR = {800, 150, 850, 3};
static const BeepPattern PATTERN_MID = {1000, 150, 350, 5};
static const BeepPattern PATTERN_NEAR = {1200, 100, 100, 10};
static const BeepPattern PATTERN_OVER = {1500, 80, 80, -1};
static const BeepPattern PATTERN_NONE = {0, 0, 0, 0};

static volatile bool beepActive = false;
static BeepPattern activePattern = PATTERN_NONE;
static unsigned long beepStartMs = 0;

void btAudioInit() {
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    Serial.printf("[SND] Buzzer ready on GPIO %d\n", BUZZER_PIN);
}

void btAudioAlert(const BeepPattern &pattern) {
    activePattern = pattern;
    beepStartMs = millis();
    beepActive = true;
    Serial.printf("[SND] Alert: %dHz on=%d off=%d repeat=%d\n", pattern.freqHz, pattern.onMs,
                  pattern.offMs, pattern.repeatCount);
}

void btAudioStop() {
    beepActive = false;
    noTone(BUZZER_PIN);
    Serial.println("[SND] Alert stopped");
}

void btAudioUpdate() {
    if (!beepActive || activePattern.freqHz == 0) {
        noTone(BUZZER_PIN);
        return;
    }

    const unsigned long elapsed = millis() - beepStartMs;
    const int cycleMs = activePattern.onMs + activePattern.offMs;
    if (cycleMs <= 0) {
        tone(BUZZER_PIN, activePattern.freqHz);
        return;
    }

    const int cycleIndex = static_cast<int>(elapsed / static_cast<unsigned long>(cycleMs));
    const int posInCycle = static_cast<int>(elapsed % static_cast<unsigned long>(cycleMs));

    if (activePattern.repeatCount >= 0 && cycleIndex >= activePattern.repeatCount) {
        beepActive = false;
        noTone(BUZZER_PIN);
        return;
    }

    if (posInCycle < activePattern.onMs) tone(BUZZER_PIN, activePattern.freqHz);
    else noTone(BUZZER_PIN);
}

// 기존 출력 포맷 호환용
bool btAudioConnected() { return true; }
bool btIsConnected() { return true; }

#endif
