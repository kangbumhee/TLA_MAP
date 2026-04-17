// ble_test.h — 실제 cameras.csv 좌표 기반 시뮬레이션
#ifndef BLE_TEST_H
#define BLE_TEST_H

#include <math.h>

#include "ble_alert.h"
#include "gps_handler.h"

struct Waypoint { double lat; double lon; };

struct TestScenario {
    const char* name;
    int camType;
    int speedLimit;
    int testSpeed;
    const Waypoint* route;
    int routeCount;
};

// ============================================================
// 시나리오 1: 과속카메라 (제한80)
// 카메라: 37.470461, 126.986922
// 경로: 남→북 접근
// ============================================================
static const Waypoint route_overspeed[] = {
    {37.4600, 126.9869},   // 시작 (남쪽 1.1km)
    {37.4620, 126.9869},
    {37.4640, 126.9869},
    {37.4660, 126.9869},
    {37.4680, 126.9870},
    {37.4700, 126.9869},   // 카메라 300m 전
    {37.4705, 126.9869},   // ★ 카메라 (37.470461)
    {37.4710, 126.9869},
    {37.4730, 126.9869},
    {37.4750, 126.9869},   // 통과 후
    {37.4780, 126.9869},
};

// ============================================================
// 시나리오 2: 구간단속 (제한60)
// 카메라: 37.482661, 127.012154
// 경로: 남→북 접근
// ============================================================
static const Waypoint route_section[] = {
    {37.4720, 127.0121},   // 시작 (남쪽 1.2km)
    {37.4740, 127.0121},
    {37.4760, 127.0122},
    {37.4780, 127.0122},
    {37.4800, 127.0121},
    {37.4820, 127.0121},   // 카메라 300m 전
    {37.4827, 127.0122},   // ★ 카메라 (37.482661)
    {37.4835, 127.0122},
    {37.4850, 127.0122},
    {37.4870, 127.0122},   // 통과 후
    {37.4900, 127.0122},
};

// ============================================================
// 시나리오 3: 신호위반 (제한50)
// 카메라: 37.470685, 126.936995
// 경로: 남→북 접근
// ============================================================
static const Waypoint route_signal[] = {
    {37.4600, 126.9370},   // 시작
    {37.4620, 126.9370},
    {37.4640, 126.9370},
    {37.4660, 126.9370},
    {37.4680, 126.9370},
    {37.4700, 126.9370},   // 카메라 300m 전
    {37.4707, 126.9370},   // ★ 카메라 (37.470685)
    {37.4715, 126.9370},
    {37.4730, 126.9370},
    {37.4750, 126.9370},   // 통과 후
    {37.4780, 126.9370},
};

// ============================================================
// 시나리오 4: 버스전용 (제한30)
// 카메라: 37.578332, 126.986769
// 경로: 남→북 (세종대로)
// ============================================================
static const Waypoint route_bus[] = {
    {37.5680, 126.9868},   // 시작
    {37.5700, 126.9868},
    {37.5720, 126.9868},
    {37.5740, 126.9868},
    {37.5760, 126.9868},
    {37.5780, 126.9868},   // ★ 카메라 (37.578332)
    {37.5790, 126.9868},
    {37.5800, 126.9868},
    {37.5820, 126.9868},   // 통과 후
    {37.5840, 126.9868},
};

// ============================================================
// 시나리오 5: 어린이보호구역 (제한50)
// 카메라: 37.514829, 127.052106
// 경로: 남→북 접근
// ============================================================
static const Waypoint route_child[] = {
    {37.5040, 127.0521},   // 시작
    {37.5060, 127.0521},
    {37.5080, 127.0521},
    {37.5100, 127.0521},
    {37.5120, 127.0521},
    {37.5145, 127.0521},   // ★ 카메라 (37.514829)
    {37.5155, 127.0521},
    {37.5170, 127.0521},
    {37.5190, 127.0521},   // 통과 후
    {37.5210, 127.0521},
};

// ============================================================
// 시나리오 6: 과속 상태로 과속카메라 접근 (제한80, 속도100)
// 같은 경로, 속도만 다름
// ============================================================

// ============================================================
// 시나리오 7: 고속도로 구간단속 (제한80)
// 카메라: 37.507042, 126.991574
// 경로: 남→북
// ============================================================
static const Waypoint route_highway[] = {
    {37.4960, 126.9916},   // 시작
    {37.4980, 126.9916},
    {37.5000, 126.9916},
    {37.5020, 126.9916},
    {37.5040, 126.9916},
    {37.5060, 126.9916},
    {37.5070, 126.9916},   // ★ 카메라 (37.507042)
    {37.5080, 126.9916},
    {37.5100, 126.9916},
    {37.5120, 126.9916},   // 통과 후
    {37.5150, 126.9916},
};

// ── 시나리오 배열 ─────────────────────────────
static const TestScenario scenarios[] = {
    {"과속카메라 (제한80, 속도75)",
     0, 80, 75, route_overspeed, sizeof(route_overspeed) / sizeof(Waypoint)},

    {"구간단속 (제한60, 속도55)",
     3, 60, 55, route_section, sizeof(route_section) / sizeof(Waypoint)},

    {"신호위반 (제한50, 속도45)",
     4, 50, 45, route_signal, sizeof(route_signal) / sizeof(Waypoint)},

    {"버스전용 (제한30, 속도25)",
     5, 30, 25, route_bus, sizeof(route_bus) / sizeof(Waypoint)},

    {"어린이보호구역 (제한50, 속도45)",
     6, 50, 45, route_child, sizeof(route_child) / sizeof(Waypoint)},

    {"과속접근! (제한80, 속도100)",
     0, 80, 100, route_overspeed, sizeof(route_overspeed) / sizeof(Waypoint)},

    {"구간단속 과속 (제한80, 속도100)",
     3, 80, 100, route_highway, sizeof(route_highway) / sizeof(Waypoint)},
};
static const int SCENARIO_COUNT = sizeof(scenarios) / sizeof(TestScenario);

// ── 시뮬 상태 ─────────────────────────────────
static bool simRunning = false;
static int simScenarioIdx = 0;
static int simWaypointIdx = 0;
static int simSpeedKmh = 60;
static double simLat = 0, simLon = 0;
static unsigned long simLastMs = 0;
static double simProgress = 0;

static int autoScenarioIdx = -1;
static unsigned long autoNextMs = 0;

static double testDistM(double lat1, double lon1, double lat2, double lon2) {
    double dLat = (lat2 - lat1) * 111320.0;
    double dLon = (lon2 - lon1) * 111320.0 * cos(lat1 * 3.14159265 / 180.0);
    return sqrt(dLat * dLat + dLon * dLon);
}

// ── 도움말 ────────────────────────────────────
void testPrintHelp() {
    Serial.println();
    Serial.println("===== 카메라 시뮬레이션 (실제 좌표) =====");
    Serial.println();
    Serial.println("  h   : 도움말");
    Serial.println("  l   : 시나리오 목록");
    Serial.println();
    Serial.println("  1   : 과속카메라 (제한80, 75km/h)");
    Serial.println("  2   : 구간단속 (제한60, 55km/h)");
    Serial.println("  3   : 신호위반 (제한50, 45km/h)");
    Serial.println("  4   : 버스전용 (제한30, 25km/h)");
    Serial.println("  5   : 어린이보호구역 (제한50, 45km/h)");
    Serial.println("  6   : 과속 접근! (제한80, 100km/h)");
    Serial.println("  7   : 구간단속 과속 (제한80, 100km/h)");
    Serial.println("  8   : 전체 순차 자동 실행");
    Serial.println();
    Serial.println("  s <속도>  : 속도 변경 (예: s 120)");
    Serial.println("  x         : 중지");
    Serial.println("  g <lat> <lon> <spd> : GPS 직접 전송");
    Serial.println("==========================================");
    Serial.println();
}

void testPrintList() {
    Serial.println("\n--- 시나리오 목록 ---");
    for (int i = 0; i < SCENARIO_COUNT; i++) {
        Serial.printf("  [%d] %s\n", i + 1, scenarios[i].name);
    }
    Serial.println();
}

// ── 시뮬 시작 ─────────────────────────────────
void testStartSim(int idx, int speedOverride = 0) {
    if (idx < 0 || idx >= SCENARIO_COUNT) {
        Serial.printf("[TEST] 번호 오류: %d\n", idx + 1);
        return;
    }

    simScenarioIdx = idx;
    simWaypointIdx = 0;
    simProgress = 0;
    simRunning = true;
    simLastMs = millis();

    const TestScenario& s = scenarios[idx];
    simSpeedKmh = (speedOverride > 0) ? speedOverride : s.testSpeed;
    simLat = s.route[0].lat;
    simLon = s.route[0].lon;

    Serial.println();
    Serial.println("==========================================");
    Serial.printf("[SIM] %s\n", s.name);
    Serial.printf("[SIM] 제한: %d km/h | 주행: %d km/h\n", s.speedLimit, simSpeedKmh);
    Serial.printf("[SIM] 경로: %d 포인트\n", s.routeCount);
    Serial.printf("[SIM] 시작: %.6f, %.6f\n", simLat, simLon);
    if (simSpeedKmh > s.speedLimit) {
        Serial.println("[SIM] *** 과속 상태 ***");
    }
    Serial.println("==========================================\n");
}

// ── 시뮬 업데이트 ─────────────────────────────
void testUpdateSim() {
    if (!simRunning || !bleIsConnected()) return;

    unsigned long now = millis();
    if (now - simLastMs < 1000) return;
    simLastMs = now;

    const TestScenario& sc = scenarios[simScenarioIdx];

    if (simWaypointIdx >= sc.routeCount - 1) {
        const Waypoint& endWp = sc.route[sc.routeCount - 1];
        simLat = endWp.lat;
        simLon = endWp.lon;
        Serial.printf("[SIM] %d/%d | %.6f, %.6f | %d km/h (종료)\n",
                      sc.routeCount, sc.routeCount, simLat, simLon, simSpeedKmh);
        bleGpsSend(simLat, simLon, 10, true);
        bleSpeedSend((uint16_t)simSpeedKmh);
        Serial.println("\n[SIM] === 경로 끝 ===\n");
        simRunning = false;
        return;
    }

    const Waypoint& wp1 = sc.route[simWaypointIdx];
    const Waypoint& wp2 = sc.route[simWaypointIdx + 1];

    double segDist = testDistM(wp1.lat, wp1.lon, wp2.lat, wp2.lon);
    if (segDist < 1) segDist = 1;

    double moveM = simSpeedKmh / 3.6;
    simProgress += moveM / segDist;

    while (simProgress >= 1.0 && simWaypointIdx < sc.routeCount - 1) {
        simProgress -= 1.0;
        simWaypointIdx++;
        if (simWaypointIdx >= sc.routeCount - 1) {
            simProgress = 1.0;
            break;
        }
    }

    int ci = simWaypointIdx;
    int ni = (ci + 1 < sc.routeCount) ? ci + 1 : ci;
    double t = simProgress;
    if (t > 1.0) t = 1.0;

    simLat = sc.route[ci].lat + (sc.route[ni].lat - sc.route[ci].lat) * t;
    simLon = sc.route[ci].lon + (sc.route[ni].lon - sc.route[ci].lon) * t;

    int wp = simWaypointIdx + 1;
    int total = sc.routeCount;

    Serial.printf("[SIM] %d/%d | %.6f, %.6f | %d km/h\n",
                  wp, total, simLat, simLon, simSpeedKmh);

    bleGpsSend(simLat, simLon, 10, true);
    bleSpeedSend((uint16_t)simSpeedKmh);

    if (simWaypointIdx >= sc.routeCount - 1) {
        Serial.println("\n[SIM] === 경로 끝 ===\n");
        simRunning = false;
    }
}

// ── 전체 순차 ─────────────────────────────────
void testStartAll() {
    autoScenarioIdx = 0;
    autoNextMs = millis();
    Serial.println("\n[TEST] === 전체 순차 시작 ===\n");
}

void testUpdateAll() {
    if (autoScenarioIdx < 0) return;
    if (!simRunning && millis() >= autoNextMs) {
        if (autoScenarioIdx >= SCENARIO_COUNT) {
            Serial.println("\n[TEST] === 전체 완료 ===\n");
            autoScenarioIdx = -1;
            return;
        }
        testStartSim(autoScenarioIdx);
        autoScenarioIdx++;
        autoNextMs = millis() + 120000;
    }
}

inline bool testIsBleOverrideActive() {
    return simRunning || autoScenarioIdx >= 0;
}

// ── 명령어 처리 ───────────────────────────────
static char cmdBuf[64];
static int cmdIdx = 0;

void testProcessCommand(const char* cmd) {
    while (*cmd == ' ') cmd++;
    if (*cmd == '\0') return;
    char c0 = cmd[0];

    if (c0 == 'h' || c0 == 'H' || c0 == '?') {
        testPrintHelp();
        return;
    }
    if (c0 == 'l' || c0 == 'L') {
        testPrintList();
        return;
    }
    if (c0 == 'x' || c0 == 'X') {
        simRunning = false;
        autoScenarioIdx = -1;
        Serial.println("[TEST] 중지\n");
        return;
    }
    if (c0 == 's' || c0 == 'S') {
        int spd = 0;
        if (sscanf(cmd + 1, "%d", &spd) == 1 && spd > 0) {
            simSpeedKmh = spd;
            Serial.printf("[TEST] 속도 → %d km/h\n", spd);
        }
        return;
    }
    if (c0 == 'g' || c0 == 'G') {
        double lat, lon;
        int spd;
        if (sscanf(cmd + 1, "%lf %lf %d", &lat, &lon, &spd) == 3) {
            Serial.printf("[TEST] GPS: %.6f, %.6f, %d\n", lat, lon, spd);
            bleGpsSend(lat, lon, 8, true);
            bleSpeedSend((uint16_t)spd);
        }
        return;
    }
    if (c0 >= '1' && c0 <= '8') {
        int n = c0 - '1';
        if (n == 7) {
            testStartAll();
            return;
        }
        if (n < SCENARIO_COUNT) {
            int spdOvr = 0;
            (void)sscanf(cmd + 1, "%d", &spdOvr);
            testStartSim(n, spdOvr);
        }
        return;
    }
    Serial.printf("[TEST] ? '%s' (h=도움말)\n", cmd);
}

void testReadSerial() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (cmdIdx > 0) {
                cmdBuf[cmdIdx] = '\0';
                testProcessCommand(cmdBuf);
                cmdIdx = 0;
            }
        } else if (cmdIdx < (int)sizeof(cmdBuf) - 1) {
            cmdBuf[cmdIdx++] = c;
        }
    }
}

void testInit() { testPrintHelp(); }

void testLoop() {
    testReadSerial();
    testUpdateSim();
    testUpdateAll();
}

#endif
