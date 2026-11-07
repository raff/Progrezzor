// Copyright (C) 2026 Henrik Juhl. Part of Progrezzor, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <atomic>

// Hands the latest value of something from one thread (the editor) to another (the audio thread) without locks, waiting
// or allocation. The writer fills its own slot and publishes it; the reader asks for news at the start of each block and,
// if there is some, swaps to the newest slot. Neither ever touches the slot that the other is using. Exactly one writer
// and one reader.
namespace engine
{
template <typename T>
class TripleBuffer
{
public:
    // ---- Writer ----
    T& writeSlot() { return slots[writeIndex]; }

    void publish()
    {
        const int previous = shared.exchange (writeIndex | kNewData, std::memory_order_acq_rel);
        writeIndex = previous & ~kNewData;
    }

    // ---- Reader ----
    // True if the writer has published something since the last call; read() then gives that.
    bool update()
    {
        if ((shared.load (std::memory_order_acquire) & kNewData) == 0)
            return false;

        const int previous = shared.exchange (readIndex, std::memory_order_acq_rel);
        readIndex = previous & ~kNewData;
        return true;
    }

    const T& read() const { return slots[readIndex]; }

private:
    static constexpr int kNewData = 4;

    T slots[3] {};
    int writeIndex = 0; // writer's
    int readIndex = 2;  // reader's
    std::atomic<int> shared { 1 }; // the slot in between, with kNewData set if the writer has published it
};
}
