// Copyright (C) 2026 Henrik Juhl. Part of Progrezzor, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "LoopPlan.h"

// Where the loop is: which block plays and how far it has got. Pure C++, no JUCE, no allocation.
//
// The loop used to work out its place from the host's position modulo the length of the progression. That makes the loop jump
// when the bar is edited while it plays: every change of length moves every later block on the timeline. Instead the cursor
// holds the identity of the block that plays and how many beats of it have been played, and counts on with the host's time. A
// change in the bar never moves the cursor; it only decides where the cursor goes after the block that plays.
//
// The position is worked out from the host's position (as before, modulo the length) only at the start, after a stop and start,
// after a jump in the host's time and when the host loops: then block 1 lies where the host's position says again.
namespace engine
{
class LoopCursor
{
public:
    void reset();

    // True while a block is playing (also one that has been deleted and plays to its end)
    bool isRunning() const { return mode == Mode::Running; }
    int currentId() const { return mode == Mode::Running && ! ghost ? curId : 0; }
    bool isOnDeletedBlock() const { return mode == Mode::Running && ghost; }

    // The bar has changed: `fresh` is what the loop plays from now on, `old` what it played. The block that plays is found again
    // in `fresh` and goes on from where it is. `restart`: the bar was replaced as a whole, so the block that plays is finished and
    // the loop goes on with block 1. `nowBeat` is the host's position at the start of the next block of audio and `halfBarBeats`
    // half a bar in the host's time signature.
    void adopt (const LoopSnapshot& old, const LoopSnapshot& fresh, bool restart, double nowBeat, double halfBarBeats);

    // What happens in a block of `numSamples` samples whose first sample is at `hostBeat`. `resync`: work the position out from the
    // host again (start, a jump, a loop in the host). Without a position (nothing to play) the plan is empty.
    LoopPlan plan (const LoopSnapshot&, double hostBeat, bool resync, double beatsPerSample, int numSamples);

private:
    enum class Mode { Idle, PreRoll, Running };

    LoopPlan start (const LoopSnapshot&, double hostBeat, double beatsPerSample, int numSamples);
    static int indexOf (const LoopSnapshot&, int id);
    // After the block that plays: the index of the block in `s` that comes next, -1 if there is none
    int resolveNext (const LoopSnapshot& s) const;
    // The first block after `fromId` in `old`'s order that is still in `fresh`; sets nextId, or nextFirst if there is none
    void findSuccessor (const LoopSnapshot& old, const LoopSnapshot& fresh, int fromId);

    Mode mode = Mode::Idle;
    int curId = 0;           // the block that plays
    double elapsed = 0.0;    // beats of it played at the start of the next block of audio
    double length = 0.0;     // beats it plays in all (the length in the bar, unless it was cut short or deleted)
    bool ghost = false;      // the block is not in the bar any more: it plays to its end
    int nextId = 0;          // after a ghost: the block that goes on (0: block 1 of the bar)
    bool nextFirst = true;
    double expectedHost = 0.0; // where the host should be at the start of the next block of audio
};
}
