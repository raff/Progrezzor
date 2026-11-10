// Copyright (C) 2026 Henrik Juhl. Part of Progrezzor, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <cmath>
#include <vector>

#include "ChordEngine.h"

// The progression: a row of blocks. Pure C++, no JUCE.
//
// A block holds a chord (distance from the tonic, type, inversion: it follows the key) and a length in beats. Lengths are
// in beats, never in bars, and the smallest length is one constant, so that a quarter of a bar can be allowed later
// without breaking old projects.
namespace engine
{
constexpr double kBeatsPerBar = 4.0;     // until the host's time signature is used (milestone 4)
constexpr double kMinBlockBeats = 2.0;   // half a bar
constexpr double kMaxBlockBeats = 32.0;  // eight bars
constexpr double kDefaultBlockBeats = 4.0;
constexpr int kMaxBlocks = 32;

struct Block
{
    Chord chord;
    double beats = kDefaultBlockBeats;
    bool locked = false; // a locked block stays when Suggest replaces the rest
    bool disabled = false; // Quick-text put a chord outside the scale here: it is silent while Lock to scale is on
    bool rest = false;     // a pause: silence for the length of the block (the chord is not used)
    // The block's identity: it lives through moves and changes (chord, length, lock), so that the loop can find the block that plays
    // again after the bar has been edited. 0 means "not given yet": the processor gives every block its own number. Not saved,
    // and not part of ==, because two blocks that look the same are the same to the user.
    int id = 0;

    friend bool operator== (const Block& a, const Block& b)
    {
        return a.chord == b.chord && std::abs (a.beats - b.beats) < 1.0e-9 && a.locked == b.locked && a.disabled == b.disabled && a.rest == b.rest;
    }
    friend bool operator!= (const Block& a, const Block& b) { return ! (a == b); }
};

using Progression = std::vector<Block>;

// True if the block is silent now: Quick-text marked it, Lock to scale is on, and the chord is not in the scale. Turn the
// lock off (or change the key so that the chord fits) and it plays. The chord itself is never changed.
inline bool isBlockDisabled (const Block& block, ScaleId scale, bool lockToScale)
{
    return block.disabled && lockToScale && ! chordFitsScale (block.chord, scale);
}

inline double totalBeats (const Progression& progression)
{
    double total = 0.0;
    for (const auto& block : progression)
        total += block.beats;
    return total;
}
}
