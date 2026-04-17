#pragma once
#include "../can_frame_types.h"
#include "can_driver.h"
#include <Arduino.h>
#include <driver/twai.h>

struct TWAIDiagnostics {
    const char *state;
    uint32_t rxErrors, txErrors, busErrors, rxMissed, rxQueued;
};

class TWAIDriver : public CanDriver {
public:
    static constexpr bool kSupportsISR = false;

    TWAIDriver(gpio_num_t tx, gpio_num_t rx) : txPin_(tx), rxPin_(rx) {}

    bool init() override {
        g_ = TWAI_GENERAL_CONFIG_DEFAULT(txPin_, rxPin_, TWAI_MODE_LISTEN_ONLY);
        g_.rx_queue_len = 64;
        g_.tx_queue_len = 0; // listen-only -> TX 불필요
        t_ = TWAI_TIMING_CONFIG_500KBITS();
        f_ = TWAI_FILTER_CONFIG_ACCEPT_ALL();
        if (twai_driver_install(&g_, &t_, &f_) != ESP_OK) return false;
        if (twai_start() != ESP_OK) {
            twai_driver_uninstall();
            return false;
        }
        ok_ = true;
        return true;
    }

    void setFilters(const uint32_t *, uint8_t) override {
        // Accept all - 모니터링 전용
    }

    bool enableInterrupt(void (*)()) override { return false; }

    bool read(CanFrame &frame) override {
        if (!ok_) {
            tryRecover();
            return false;
        }

        twai_message_t msg;
        if (twai_receive(&msg, 0) != ESP_OK) {
            if (isBusOff()) recoverWithCooldown();
            return false;
        }

        frame.id = msg.identifier;
        frame.dlc = (msg.data_length_code <= 8) ? msg.data_length_code : 8;
        memset(frame.data, 0, 8);
        memcpy(frame.data, msg.data, frame.dlc);
        return true;
    }

    void send(const CanFrame &) override {
        // listen-only: 전송 비활성화 (안전)
    }

    TWAIDiagnostics getDiagnostics() {
        TWAIDiagnostics d = {"UNKNOWN", 0, 0, 0, 0, 0};
        twai_status_info_t s;
        if (twai_get_status_info(&s) != ESP_OK) return d;

        switch (s.state) {
            case TWAI_STATE_STOPPED: d.state = "STOPPED"; break;
            case TWAI_STATE_RUNNING: d.state = "RUNNING"; break;
            case TWAI_STATE_BUS_OFF: d.state = "BUS_OFF"; break;
            case TWAI_STATE_RECOVERING: d.state = "RECOVERING"; break;
            default: break;
        }

        d.rxErrors = s.rx_error_counter;
        d.txErrors = s.tx_error_counter;
        d.busErrors = s.bus_error_count;
        d.rxMissed = s.rx_missed_count;
        d.rxQueued = s.msgs_to_rx;
        return d;
    }

private:
    static constexpr uint32_t COOLDOWN = 1000;

    bool isBusOff() {
        twai_status_info_t s;
        return (twai_get_status_info(&s) == ESP_OK && s.state == TWAI_STATE_BUS_OFF);
    }

    void recoverWithCooldown() {
        uint32_t now = millis();
        if (now - lastRec_ < COOLDOWN) return;
        lastRec_ = now;
        twai_stop();
        twai_driver_uninstall();
        ok_ = (twai_driver_install(&g_, &t_, &f_) == ESP_OK && twai_start() == ESP_OK);
    }

    void tryRecover() {
        uint32_t now = millis();
        if (now - lastRec_ < COOLDOWN * 10) return;
        lastRec_ = now;
        ok_ = (twai_driver_install(&g_, &t_, &f_) == ESP_OK && twai_start() == ESP_OK);
    }

    gpio_num_t txPin_, rxPin_;
    twai_general_config_t g_{};
    twai_timing_config_t t_{};
    twai_filter_config_t f_{};
    bool ok_ = false;
    uint32_t lastRec_ = 0;
};
