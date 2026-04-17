#pragma once
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <cmath>
#include "gps_handler.h"
#include "sketch_config.h"

struct CameraRecord {
    int32_t latE6;
    int32_t lngE6;
    uint8_t speedLimit;
    int16_t heading;
    uint8_t type;
};

struct CameraAlert {
    bool   active = false;
    int    phase = 0;
    int    prevPhase = 0;
    int    distanceM = 9999;
    int    speedLimit = 0;
    uint8_t camType = 0;
    bool   overspeed = false;
    float  camLat = 0, camLng = 0;
};

inline CameraAlert camAlert;

class SpeedCameraDB {
public:
    // 주변 카메라 버퍼 (메모리에 올리는 양)
    static constexpr int MAX_NEARBY = 500;
    CameraRecord nearby[MAX_NEARBY];
    int nearbyCount = 0;

    // SD 카드 전체 카메라 수
    int totalCount = 0;

    // 마지막 로드 위치
    double lastLoadLat = 0, lastLoadLng = 0;
    uint32_t lastLoadMs = 0;

    // SD 마운트 상태
    bool sdReady = false;
    SPIClass *hspi = nullptr;

    bool init() {
        hspi = new SPIClass(HSPI);
        hspi->begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
        if (!SD.begin(SD_CS, *hspi, 1000000)) {
            Serial.println("[SD] Mount failed");
            return false;
        }
        sdReady = true;

        // 파일 존재 확인 + 크기로 줄 수 추정
        File f = SD.open("/cameras.csv", FILE_READ);
        if (!f) {
            Serial.println("[SD] cameras.csv not found");
            return false;
        }

        size_t fileSize = f.size();
        f.close();

        // 한 줄 평균 약 30바이트로 추정 (헤더 제외)
        totalCount = static_cast<int>(fileSize / 30);
        if (totalCount < 1) totalCount = 1;

        Serial.printf("[CAM] SD file: %d bytes, ~%d cameras\n",
                      static_cast<int>(fileSize), totalCount);
        Serial.printf("[CAM] Free heap: %d bytes\n", ESP.getFreeHeap());
        return true;
    }

    // GPS 위치 기반으로 주변 카메라만 로드
    void loadNearby(double lat, double lng) {
        if (!sdReady) return;

        // 이전 로드 위치에서 3km 이상 이동했거나, 처음이면 로드
        if (lastLoadLat != 0) {
            int moved = fastDistM(lat, lng, lastLoadLat, lastLoadLng);
            if (moved < 3000) return;  // 3km 미만 이동 → 스킵
        }

        uint32_t startMs = millis();

        File f = SD.open("/cameras.csv", FILE_READ);
        if (!f) return;

        // 헤더 스킵
        char line[128];
        readLine(f, line, sizeof(line));

        nearbyCount = 0;
        int scanned = 0;

        // 검색 범위: 위도 ±0.05 (약 ±5.5km), 경도 ±0.065 (약 ±5.5km)
        int32_t latE6 = (int32_t)(lat * 1000000.0);
        int32_t lngE6 = (int32_t)(lng * 1000000.0);
        int32_t latRange = 50000;   // 0.05도
        int32_t lngRange = 65000;   // 0.065도

        while (f.available() && nearbyCount < MAX_NEARBY) {
            if (readLine(f, line, sizeof(line)) <= 0) continue;
            scanned++;

            float rlat, rlng;
            int spd, hdg, typ;
            if (!parseLine(line, rlat, rlng, spd, hdg, typ)) continue;

            int32_t cLatE6 = (int32_t)(rlat * 1000000.0f);
            int32_t cLngE6 = (int32_t)(rlng * 1000000.0f);

            // 사각형 필터
            if (abs(cLatE6 - latE6) > latRange) continue;
            if (abs(cLngE6 - lngE6) > lngRange) continue;

            nearby[nearbyCount].latE6 = cLatE6;
            nearby[nearbyCount].lngE6 = cLngE6;
            nearby[nearbyCount].speedLimit = (uint8_t)spd;
            nearby[nearbyCount].heading = (int16_t)hdg;
            nearby[nearbyCount].type = (uint8_t)typ;
            nearbyCount++;
        }

        f.close();

        lastLoadLat = lat;
        lastLoadLng = lng;
        lastLoadMs = millis();

        uint32_t elapsed = lastLoadMs - startMs;
        Serial.printf("[CAM] Loaded %d nearby (scanned %d, %dms)\n",
                       nearbyCount, scanned, elapsed);
    }

    // 경고 체크 (메모리의 nearby만 검색 — 빠름)
    void check(double lat, double lng, float speedKmh, float heading) {
        if (nearbyCount == 0) { camAlert.active = false; return; }

        int nearestDist = 99999;
        int nearestIdx = -1;

        int32_t latE6 = (int32_t)(lat * 1000000.0);
        int32_t lngE6 = (int32_t)(lng * 1000000.0);

        for (int i = 0; i < nearbyCount; i++) {
            // 초고속 사각형 필터 (약 ±700m)
            int32_t dLat = abs(latE6 - nearby[i].latE6);
            int32_t dLng = abs(lngE6 - nearby[i].lngE6);
            if (dLat > 6500 || dLng > 8000) continue;

            // 방향 체크
            if (nearby[i].heading >= 0) {
                float diff = fabs(heading - nearby[i].heading);
                if (diff > 180) diff = 360 - diff;
                if (diff > CAM_HEADING_TOL) continue;
            }

            int d = haversineM(lat, lng,
                              nearby[i].latE6 / 1000000.0,
                              nearby[i].lngE6 / 1000000.0);

            if (d < nearestDist) {
                nearestDist = d;
                nearestIdx = i;
            }
        }

        camAlert.prevPhase = camAlert.phase;

        if (nearestIdx < 0 || nearestDist > CAM_WARN_FAR) {
            camAlert.active = false;
            camAlert.phase = 0;
            camAlert.camType = 0;
            return;
        }

        camAlert.active = true;
        camAlert.distanceM = nearestDist;
        camAlert.speedLimit = nearby[nearestIdx].speedLimit;
        camAlert.camType = nearby[nearestIdx].type;
        camAlert.camLat = nearby[nearestIdx].latE6 / 1000000.0f;
        camAlert.camLng = nearby[nearestIdx].lngE6 / 1000000.0f;
        camAlert.overspeed = (nearby[nearestIdx].speedLimit > 0)
                           && (speedKmh > nearby[nearestIdx].speedLimit);

        if (nearestDist <= CAM_WARN_PASS)      camAlert.phase = 3;
        else if (nearestDist <= CAM_WARN_NEAR)  camAlert.phase = 2;
        else                                     camAlert.phase = 1;
    }

private:
    int readLine(File &f, char *buf, int maxLen) {
        int i = 0;
        while (f.available() && i < maxLen - 1) {
            char c = f.read();
            if (c == '\n') break;
            if (c != '\r') buf[i++] = c;
        }
        buf[i] = '\0';
        return i;
    }

    bool parseLine(const char *line, float &lat, float &lng, int &spd, int &hdg, int &typ) {
        char tmp[128];
        strncpy(tmp, line, 127); tmp[127] = '\0';
        char *tok = strtok(tmp, ",");
        if (!tok) return false; lat = atof(tok);
        tok = strtok(NULL, ","); if (!tok) return false; lng = atof(tok);
        tok = strtok(NULL, ","); if (!tok) return false; spd = atoi(tok);
        tok = strtok(NULL, ","); if (!tok) return false; hdg = atoi(tok);
        tok = strtok(NULL, ","); typ = tok ? atoi(tok) : 0;
        return (lat > 33.0f && lat < 39.0f && lng > 124.0f && lng < 132.0f);
    }

    static int fastDistM(double lat1, double lon1, double lat2, double lon2) {
        double dLat = (lat2 - lat1) * 111320.0;
        double dLon = (lon2 - lon1) * 111320.0 * cos(lat1 * 3.14159265 / 180.0);
        return (int)sqrt(dLat * dLat + dLon * dLon);
    }

    static int haversineM(double lat1, double lon1, double lat2, double lon2) {
        constexpr double R = 6371000.0;
        double dLat = degToRad(lat2 - lat1);
        double dLon = degToRad(lon2 - lon1);
        double a = sin(dLat/2)*sin(dLat/2) +
                   cos(degToRad(lat1))*cos(degToRad(lat2))*sin(dLon/2)*sin(dLon/2);
        return (int)(R * 2.0 * atan2(sqrt(a), sqrt(1-a)));
    }

    static double degToRad(double deg) { return deg * 3.14159265358979323846 / 180.0; }
};

inline SpeedCameraDB cameraDB;
