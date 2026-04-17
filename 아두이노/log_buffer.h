#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "shared_types.h"

struct LogRingBuffer {
    static constexpr size_t kCapacity = 64;
    static constexpr size_t kMaxMsgLen = 128;

    struct Entry {
        char msg[kMaxMsgLen];
        uint32_t timestamp_ms;
    };

    Entry entries[kCapacity] = {};
    Shared<uint32_t> head{0};

    void push(const char *msg, uint32_t ts) {
        uint32_t h = loadShared(head);
        Entry &e = entries[h % kCapacity];
        strncpy(e.msg, msg, kMaxMsgLen - 1);
        e.msg[kMaxMsgLen - 1] = '\0';
        e.timestamp_ms = ts;
        storeShared(head, h + 1);
    }

    uint32_t currentHead() const { return loadShared(head); }
    const Entry &at(uint32_t idx) const { return entries[idx % kCapacity]; }

    int readSince(uint32_t since, Entry *out, int maxOut) const {
        uint32_t h = loadShared(head);
        uint32_t oldest = (h > kCapacity) ? (h - kCapacity) : 0;
        uint32_t start = (since > oldest) ? since : oldest;
        int count = 0;
        for (uint32_t i = start; i < h && count < maxOut; i++) {
            out[count++] = entries[i % kCapacity];
        }
        return count;
    }
};

inline LogRingBuffer logRing;
