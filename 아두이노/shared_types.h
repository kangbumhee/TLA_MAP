#pragma once

#ifdef NATIVE_BUILD
template <typename T> using Shared = T;
template <typename T> constexpr T loadShared(const T &v) { return v; }
template <typename T> constexpr void storeShared(T &target, const T &v) { target = v; }
#else
#include <atomic>
template <typename T> using Shared = std::atomic<T>;
template <typename T> T loadShared(const std::atomic<T> &v) {
    return v.load(std::memory_order_relaxed);
}
template <typename T> void storeShared(std::atomic<T> &target, T v) {
    target.store(v, std::memory_order_relaxed);
}
#endif
