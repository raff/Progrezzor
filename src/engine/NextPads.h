// Copyright (C) 2026 Henrik Juhl. Part of Progrezzor, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <array>

#include "ChordEngine.h"

// Which pads fit well after the chord that was played last. Pure C++, no JUCE.
//
// Written by hand for this project: one small table per scale, indexed by the pad that was played last (0 to 6). Each
// row lists the best follow-ups, best first, and never more than three. Only a hint: every pad always works.
namespace engine
{
constexpr int kMaxGlowingPads = 3;

struct NextPads
{
    std::array<int, kMaxGlowingPads> pads {}; // pad numbers 0 to 6, best first
    int count = 0;
};

namespace detail
{
// [scale][last pad][up to three follow-ups, -1 for none]
constexpr int nextPadTable[kNumScales][kScaleSize][kMaxGlowingPads] = {
    // Minor
    { { 3, 5, 6 }, { 4, 0, -1 }, { 5, 3, 6 }, { 6, 0, 5 }, { 0, 5, -1 }, { 6, 2, 3 }, { 0, 2, 5 } },
    // Major
    { { 3, 4, 5 }, { 4, 0, -1 }, { 5, 3, -1 }, { 4, 0, 5 }, { 0, 5, 3 }, { 3, 1, 4 }, { 0, 2, -1 } },
    // Dorian
    { { 3, 6, 4 }, { 3, 0, -1 }, { 6, 3, 0 }, { 0, 6, 4 }, { 0, 3, 6 }, { 6, 0, -1 }, { 3, 0, 2 } },
    // Phrygian
    { { 1, 3, 6 }, { 0, 6, 2 }, { 3, 5, 1 }, { 0, 5, 6 }, { 5, 0, -1 }, { 3, 6, 0 }, { 0, 5, 3 } },
};
}

// The pads that fit after `last`. The chord counts through its root, so a pad with another kind of chord (B7 on the B
// pad) counts as that pad. A chord whose root is not a pad of the scale gives none.
inline NextPads nextPadsAfter (const Chord& last, ScaleId scale)
{
    NextPads result;
    for (int pad = 0; pad < kScaleSize; ++pad)
    {
        if (padChord (scale, pad).root != last.root)
            continue;

        for (int n : detail::nextPadTable[(int) scale][pad])
            if (n >= 0 && result.count < kMaxGlowingPads)
                result.pads[(size_t) result.count++] = n;
        break;
    }
    return result;
}
}
