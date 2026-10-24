// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef SPSC_RING_HPP
#define SPSC_RING_HPP

#include <array>
#include <atomic>
#include <cstddef>

/*
 * Single-producer single-consumer ring of N trivially copyable values. push() and
 * pop() are wait-free and never allocate, so either end may be the RT thread.
 * Exactly one thread may push and exactly one may pop.
 */
template <typename T, size_t N>
class SpscRing {
public:
    bool push(const T& v)
    {
        const size_t t = tail.load(std::memory_order_relaxed);
        if (t - head.load(std::memory_order_acquire) == N) return false;
        buf[t % N] = v;
        tail.store(t + 1, std::memory_order_release);
        return true;
    }

    bool pop(T& v)
    {
        const size_t h = head.load(std::memory_order_relaxed);
        if (h == tail.load(std::memory_order_acquire)) return false;
        v = buf[h % N];
        head.store(h + 1, std::memory_order_release);
        return true;
    }

private:
    std::array<T, N>    buf{};
    std::atomic<size_t> head{0};
    std::atomic<size_t> tail{0};
};

#endif
