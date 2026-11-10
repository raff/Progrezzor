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
    int id = 0;            // the block's identity (see Block::id)
};

// A copy of the progression that the audio thread can read, fixed in size so that no allocation is needed
struct LoopSnapshot
{
    int count = 0;
    double total = 0.0; // beats
    // A try-out of another chord on the block with this identity (see the processor): if the loop is on that block right now, it
    // switches at once. 0 for none.
    int swapNowId = 0;
    // Goes up when a whole new progression replaces the old one (Clear, Suggest, Quick-text): the loop finishes the block that
    // plays and starts again from block 1.
    int restartSerial = 0;
    std::array<LoopBlock, kMaxBlocks> blocks {};
};

inline LoopSnapshot makeSnapshot (const Progression& progression)
{
    LoopSnapshot snapshot;
    for (const auto& block : progression)
    {
        if (snapshot.count == kMaxBlocks)
            break;
        snapshot.blocks[(size_t) snapshot.count++] = { block.chord, block.beats, block.disabled, block.rest, block.id };
        snapshot.total += block.beats;
    }
    return snapshot;
}

constexpr int kMaxLoopChanges = 64;

struct LoopChange
{
    int sample;  // where in the block, 0 to numSamples - 1
    int index;   // the block that starts there, -1 if the loop stops there (the bar was emptied)
    double startBeat = 0.0; // the host's position of the block line, exactly
};

struct LoopPlan
{
    int startIndex = -1; // the block that plays at the first sample, -1 if none (before the start of the timeline)
    bool startsOnLine = false; // the first sample is the start of a block (so it sounds anew, also if the chord is the same)
    bool playing = false;      // a block sounds at the first sample (also one that has been deleted and plays to its end: startIndex is then -1)
    double startElapsed = 0.0;   // how far into that block the first sample is, in beats
    double startBlockBeat = 0.0; // the host's position of the start of that block
    double startBlockBeats = 0.0; // its length, as it is played now
    int numChanges = 0;  // block changes after the first sample, in order
    std::array<LoopChange, kMaxLoopChanges> changes {};
};

// What happens in a block of `numSamples` samples that starts at `startBeat` (quarter notes) and in which one sample is
// `beatsPerSample` of a quarter note. A change lands on the first sample that is at or after the block's start line, never
// before it. Before the start of the timeline (startBeat below 0) nothing plays until beat 0.
LoopPlan planLoop (const LoopSnapshot&, double startBeat, double beatsPerSample, int numSamples);
}
