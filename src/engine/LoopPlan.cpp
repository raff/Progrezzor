// Copyright (C) 2026 Henrik Juhl. Part of Progrezzor, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only
#include "LoopPlan.h"

#include <algorithm>
#include <cmath>

namespace engine
{
namespace
{
constexpr double kBeatTolerance = 1.0e-9;   // a position this close below a block line is counted as being on it
constexpr double kSampleTolerance = 1.0e-3; // rounding noise (a host gives its position with limited precision) in "which sample is the line on"
}

LoopPlan planLoop (const LoopSnapshot& snapshot, double startBeat, double beatsPerSample, int numSamples)
{
    LoopPlan plan;
    if (snapshot.count <= 0 || snapshot.total <= 0.0 || beatsPerSample <= 0.0 || numSamples <= 0 || ! std::isfinite (startBeat))
        return plan;

    // The beats from the start of this block to the next block line, and the block that starts there
    double toNextLine = 0.0;
    int nextIndex = 0;

    if (startBeat < 0.0)
    {
        toNextLine = -startBeat; // the loop starts at beat 0
        plan.startsOnLine = true;
    }
    else
    {
        double phase = std::fmod (startBeat, snapshot.total);
        int index = 0;
        double lineEnd = snapshot.blocks[0].beats;

        // The block that is playing: the first whose end is not reached yet
        while (phase >= lineEnd - kBeatTolerance && index < snapshot.count)
        {
            ++index;
            if (index < snapshot.count)
                lineEnd += snapshot.blocks[(size_t) index].beats;
        }

        if (index == snapshot.count) // just at the end of the loop: it starts over
        {
            index = 0;
            phase -= snapshot.total;
            lineEnd = snapshot.blocks[0].beats;
        }

        plan.startIndex = index;
        plan.playing = true;
        plan.startElapsed = phase - (lineEnd - snapshot.blocks[(size_t) index].beats);
        plan.startBlockBeat = startBeat - plan.startElapsed;
        plan.startBlockBeats = snapshot.blocks[(size_t) index].beats;
        plan.startsOnLine = (phase - (lineEnd - snapshot.blocks[(size_t) index].beats)) / beatsPerSample < 1.0 - kSampleTolerance;
        toNextLine = lineEnd - phase;
        nextIndex = (index + 1) % snapshot.count;
    }

    while (plan.numChanges < kMaxLoopChanges)
    {
        const int sample = std::max (0, (int) std::ceil (toNextLine / beatsPerSample - kSampleTolerance));
        if (sample >= numSamples)
            break;

        plan.changes[(size_t) plan.numChanges++] = { sample, nextIndex, startBeat + toNextLine };
        toNextLine += snapshot.blocks[(size_t) nextIndex].beats;
        nextIndex = (nextIndex + 1) % snapshot.count;
    }

    return plan;
}
}
