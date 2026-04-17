#pragma once
#include <Arduino.h>
#include <cmath>
#include "can_frame_types.h"

// joshwardell/model3dbc + tuncasoftbildik/tesla-can-mod 참고
struct TeslaData {
    // -- 속도/주행 --
    float vehicleSpeed = 0; // km/h
    float wheelSpeedFL = 0; // km/h
    float wheelSpeedFR = 0;
    float wheelSpeedRL = 0;
    float wheelSpeedRR = 0;
    float steeringAngle = 0; // degrees
    uint8_t gear = 0;        // 0=P 1=R 2=N 3=D
    bool brakeApplied = false;
    float accelPedal = 0;    // 0-100%
    float brakePedal = 0;    // 0-100%

    // -- 배터리 (tuncasoftbildik 디코더) --
    float packVoltage = 0; // V
    float packCurrent = 0; // A (음수=방전)
    float packPowerKW = 0; // kW
    float socPercent = 0;  // %
    float tempMin = 0;     // °C
    float tempMax = 0;     // °C
    float whPerKm = 0;     // Wh/km

    // -- 도어/트렁크 --
    bool doorFL = false, doorFR = false;
    bool doorRL = false, doorRR = false;
    bool trunkOpen = false, frunkOpen = false;

    // -- TPMS --
    float tpmsFL = 0, tpmsFR = 0, tpmsRL = 0, tpmsRR = 0; // bar

    // -- HVAC --
    float cabinTemp = 0;   // °C
    float outsideTemp = 0; // °C

    // -- 주행거리 --
    float odometer = 0; // km

    // -- 전력 --
    float frontMotorPower = 0; // kW
    float rearMotorPower = 0;  // kW
    float regenPower = 0;      // kW

    // -- 시스템 --
    uint32_t frameCount = 0;
    uint32_t lastUpdateMs = 0;
};

inline TeslaData carData;

inline void decodeTeslaCAN(const CanFrame &f) {
    carData.frameCount++;
    carData.lastUpdateMs = millis();

    switch (f.id) {
        // -- 0x108 (264) DI_torque2 -> 차량 속도 --
        case 0x108: {
            uint16_t raw = (static_cast<uint16_t>(f.data[3] & 0x0F) << 8) | f.data[2];
            carData.vehicleSpeed = raw * 0.08f;
            break;
        }

        // -- 0x129 (297) ESP_wheelSpeeds --
        case 0x129: {
            uint16_t fl = (static_cast<uint16_t>(f.data[1] << 8 | f.data[0])) & 0x1FFF;
            uint16_t fr = (static_cast<uint16_t>(f.data[3] << 3 | (f.data[2] >> 5))) & 0x1FFF;
            uint16_t rl = (static_cast<uint16_t>(f.data[4] << 6 | (f.data[3] >> 2))) & 0x1FFF;
            uint16_t rr = (static_cast<uint16_t>(f.data[6] << 1 | (f.data[5] >> 7))) & 0x1FFF;
            carData.wheelSpeedFL = fl * 0.04f;
            carData.wheelSpeedFR = fr * 0.04f;
            carData.wheelSpeedRL = rl * 0.04f;
            carData.wheelSpeedRR = rr * 0.04f;
            break;
        }

        // -- 0x132 (306) BMS_hvBusStatus -> 배터리 전압/전류 --
        case 0x132: {
            uint16_t rawV = static_cast<uint16_t>(f.data[1] << 8) | f.data[0];
            int16_t rawI = static_cast<int16_t>((f.data[3] << 8) | f.data[2]);
            carData.packVoltage = rawV * 0.01f;
            carData.packCurrent = rawI * 0.1f;
            carData.packPowerKW = (carData.packVoltage * carData.packCurrent) / 1000.0f;
            break;
        }

        // -- 0x145 (325) ESP_status -> 브레이크 --
        case 0x145: {
            carData.brakeApplied = (f.data[3] >> 7) & 0x01;
            break;
        }

        // -- 0x118 (280) DI_accelPedalPos --
        case 0x118: {
            carData.accelPedal = f.data[0] * 0.4f;
            carData.brakePedal = f.data[1] * 0.4f;
            break;
        }

        // -- 0x102 (258) VCLEFT_doorStatus --
        case 0x102: {
            carData.doorFL = (f.data[0] & 0x0F) != 0;
            carData.doorRL = ((f.data[0] >> 4) & 0x0F) != 0;
            break;
        }

        // -- 0x103 (259) VCRIGHT_doorStatus --
        case 0x103: {
            carData.doorFR = (f.data[0] & 0x0F) != 0;
            carData.doorRR = ((f.data[0] >> 4) & 0x0F) != 0;
            carData.trunkOpen = (f.data[7] & 0x0F) != 0;
            break;
        }

        // -- 0x123 (291) UI_alertMatrix1 -> 트렁크/프렁크 --
        case 0x123: {
            carData.trunkOpen = (f.data[0] >> 2) & 0x01;
            carData.frunkOpen = (f.data[0] >> 3) & 0x01;
            break;
        }

        // -- 0x292 (658) BMS_socStatus -> SOC --
        case 0x292: {
            uint16_t rawSoc = (static_cast<uint16_t>(f.data[1] & 0x03) << 8) | f.data[0];
            carData.socPercent = rawSoc * 0.1f;
            break;
        }

        // -- 0x312 (786) BMS_thermalStatus -> 배터리 온도 --
        case 0x312: {
            carData.tempMin = static_cast<float>(f.data[4]) - 40.0f;
            carData.tempMax = static_cast<float>(f.data[5]) - 40.0f;
            break;
        }

        // -- 0x33A (826) UI_ratedConsumption -> Wh/km --
        case 0x33A: {
            uint16_t raw = static_cast<uint16_t>(f.data[1] << 8) | f.data[0];
            carData.whPerKm = raw * 0.625f;
            break;
        }

        // -- 0x3B6 (950) UI_odometer --
        case 0x3B6: {
            uint32_t raw = static_cast<uint32_t>(f.data[0]) |
                           (static_cast<uint32_t>(f.data[1]) << 8) |
                           (static_cast<uint32_t>(f.data[2]) << 16) |
                           (static_cast<uint32_t>(f.data[3] & 0x0F) << 24);
            carData.odometer = raw * 0.001f;
            break;
        }

        // -- 0x261 (609) DI_state -> 기어 --
        case 0x261: {
            carData.gear = f.data[0] & 0x07;
            break;
        }

        // -- 0x2A8 (680) VCFRONT_sensors -> 실내/외부 온도 --
        case 0x2A8: {
            carData.cabinTemp = static_cast<float>(f.data[0]) * 0.5f - 40.0f;
            carData.outsideTemp = static_cast<float>(f.data[1]) * 0.5f - 40.0f;
            break;
        }

        // -- 0x3AA (938) TPMS --
        case 0x3AA: {
            carData.tpmsFL = f.data[0] * 0.02f;
            carData.tpmsFR = f.data[1] * 0.02f;
            carData.tpmsRL = f.data[2] * 0.02f;
            carData.tpmsRR = f.data[3] * 0.02f;
            break;
        }

        // -- 0x13E (318) SteeringAngle --
        case 0x13E: {
            int16_t raw = static_cast<int16_t>((f.data[1] << 8) | f.data[0]);
            carData.steeringAngle = raw * 0.1f;
            break;
        }

        default: break;
    }
}
