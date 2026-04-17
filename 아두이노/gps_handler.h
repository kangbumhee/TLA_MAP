#pragma once
#include <Arduino.h>
#include <cmath>
#include <cstdlib>
#include <cstring>

struct GpsData {
    double lat = 0, lng = 0;
    float speedKmh = 0;
    float heading = 0;
    int satellites = 0;
    bool valid = false;
    uint32_t lastFixMs = 0;
};

inline GpsData gpsData;

class GpsParser {
public:
    void feed(char c) {
        if (c == '$') {
            idx_ = 0;
        }
        if (idx_ < static_cast<int>(sizeof(buf_)) - 1) {
            buf_[idx_++] = c;
        }
        if (c == '\n') {
            buf_[idx_] = '\0';
            parse();
            idx_ = 0;
        }
    }

private:
    char buf_[128]{};
    int idx_ = 0;
    char *fields_[20]{};

    void parse() {
        if (strncmp(buf_, "$GPRMC", 6) == 0 || strncmp(buf_, "$GNRMC", 6) == 0) {
            parseRMC();
        } else if (strncmp(buf_, "$GPGGA", 6) == 0 || strncmp(buf_, "$GNGGA", 6) == 0) {
            parseGGA();
        }
    }

    int splitFields() {
        int n = 0;
        char *p = buf_;
        while (*p && n < 20) {
            fields_[n++] = p;
            while (*p && *p != ',') p++;
            if (*p == ',') *p++ = '\0';
        }
        return n;
    }

    void parseRMC() {
        int n = splitFields();
        if (n < 10) return;
        if (fields_[2][0] != 'A') {
            gpsData.valid = false;
            return;
        }

        gpsData.lat = nmeaToDecimal(fields_[3], fields_[4][0]);
        gpsData.lng = nmeaToDecimal(fields_[5], fields_[6][0]);
        gpsData.speedKmh = atof(fields_[7]) * 1.852f; // knots -> km/h
        gpsData.heading = atof(fields_[8]);
        gpsData.valid = true;
        gpsData.lastFixMs = millis();
    }

    void parseGGA() {
        int n = splitFields();
        if (n < 8) return;
        gpsData.satellites = atoi(fields_[7]);
    }

    static double nmeaToDecimal(const char *val, char dir) {
        double raw = atof(val);
        int deg = static_cast<int>(raw / 100);
        double min = raw - deg * 100.0;
        double dec = deg + min / 60.0;
        if (dir == 'S' || dir == 'W') dec = -dec;
        return dec;
    }
};

inline GpsParser gpsParser;
