// Copyright (C) 2026 Henrik Juhl. Part of Progrezzor, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <array>
#include <cmath>

#include "Progression.h"

// The loop: which block of the progression plays at which position of the host's timeline. Pure C++, no JUCE, no
// allocation.
//
// The loop is locked to the host's position (PPQ, in quarter notes), counted from the start of the timeline. The
// progression repeats every `total` beats, so the position inside it is the host position modulo that length. Because
// everything is worked out from the position the host gives at the start of each block, a loop in the DAW, a jump in
// time or a change of tempo needs no special care: the next block simply is where the host says it is.
namespace engine
{
struct LoopBlock
{
    Chord chord;
    double beats = kDefaultBlockBeats;
    bool disabled = false; // see isBlockDisabled(): the audio thread checks it against the key and the lock
    bool rest = false;     // a pause: nothing sounds
};

// A copy of the progression that the audio thread can read, fixed in size so that no allocation is needed
struct LoopSnapshot
{
    int count = 0;
    double total = 0.0; // beats
    // A try-out of another chord on this block (see the processor): if the loop is on this block right now, it switches at
    // once instead of waiting for the next block line. -1 for an ordinary change, which waits for the next block line.
    int swapNowBlock = -1;
    std::array<LoopBlock, kMaxBlocks> blocks {};
};

inline LoopSnapshot makeSnapshot (const Progression& progression)
{
    LoopSnapshot snapshot;
    for (const auto& block : progression)
    {
        if (snapshot.count == kMaxBlocks)
            break;
        snapshot.blocks[(size_t) snapshot.count++] = { block.chord, block.beats, block.disabled, block.rest };
        snapshot.total += block.beats;
    }
    return snapshot;
}

// Same blocks, same lengths (only chords may differ): a try-out can then take effect in the middle of a block
inline bool sameShape (const LoopSnapshot& a, const LoopSnapshot& b)
{
    if (a.count != b.count)
        return false;
    for (int i = 0; i < a.count; ++i)
        if (std::abs (a.blocks[(size_t) i].beats - b.blocks[(size_t) i].beats) > 1.0e-9)
            return false;
    return true;
}

constexpr int kMaxLoopChanges = 64;

struct LoopChange
{
    int sample; // where in the block, 0 to numSamples - 1
    int index;  // the block that starts there
};

struct LoopPlan
{
    int startIndex = -1; // the block that plays at the first sample, -1 if none (before the start of the timeline)
    bool startsOnLine = false; // the first sample is the start of a block (so it sounds anew, also if the chord is the same)
    int numChanges = 0;  // block changes after the first sample, in order
    std::array<LoopChange, kMaxLoopChanges> changes {};
};

// What happens in a block of `numSamples` samples that starts at `startBeat` (quarter notes) and in which one sample is
// `beatsPerSample` of a quarter note. A change lands on the first sample that is at or after the block's start line, never
// before it. Before the start of the timeline (startBeat below 0) nothing plays until beat 0.
LoopPlan planLoop (const LoopSnapshot&, double startBeat, double beatsPerSample, int numSamples);
}
