// Copyright (C) 2026 Henrik Juhl. Part of Progrezzor, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only
#include "LoopCursor.h"

#include <algorithm>
#include <cmath>

namespace engine
{
namespace
{
constexpr double kSampleTolerance = 1.0e-3; // rounding noise in "which sample is the line on"
constexpr double kBeatEpsilon = 1.0e-6;
}

void LoopCursor::reset()
{
    mode = Mode::Idle;
    curId = 0;
    elapsed = 0.0;
    length = 0.0;
    ghost = false;
    nextId = 0;
    nextFirst = true;
}

int LoopCursor::indexOf (const LoopSnapshot& s, int id)
{
    if (id == 0)
        return -1;
    for (int i = 0; i < s.count; ++i)
        if (s.blocks[(size_t) i].id == id)
            return i;
    return -1;
}

void LoopCursor::findSuccessor (const LoopSnapshot& old, const LoopSnapshot& fresh, int fromId)
{
    nextFirst = true;
    nextId = 0;
    const int from = indexOf (old, fromId);
    if (from < 0)
        return;
    for (int k = 1; k < old.count; ++k)
    {
        const int id = old.blocks[(size_t) ((from + k) % old.count)].id;
        if (indexOf (fresh, id) >= 0)
        {
            nextId = id;
            nextFirst = false;
            return;
        }
    }
}

void LoopCursor::adopt (const LoopSnapshot& old, const LoopSnapshot& fresh, bool restart, double nowBeat, double halfBarBeats)
{
    if (mode != Mode::Running)
        return; // nothing plays: the next plan starts from the host's position

    const int here = indexOf (fresh, curId);

    if (restart)
    {
        ghost = true; // the block that plays is finished, then block 1
        nextFirst = true;
        nextId = 0;
        return;
    }

    if (ghost && here < 0)
    {
        // Still deleted. If the block that was to come next has gone too, the one after it does.
        if (! nextFirst && indexOf (fresh, nextId) < 0)
            findSuccessor (old, fresh, nextId);
        return;
    }

    if (here < 0) // the block that plays was deleted: it plays to its end, and the block that now comes after it goes on
    {
        ghost = true;
        findSuccessor (old, fresh, curId);
        return;
    }

    ghost = false;
    const double newLength = fresh.blocks[(size_t) here].beats;
    if (newLength > elapsed + kBeatEpsilon)
    {
        length = newLength; // longer, or shorter but not yet played: it ends where the bar says
        return;
    }

    // Shorter than what has been played: it ends at the next half bar (or bar) line of the host's time
    const double halfBar = std::max (0.5, halfBarBeats);
    const double endAt = std::ceil (nowBeat / halfBar - 1.0e-9) * halfBar;
    length = std::max (endAt - (nowBeat - elapsed), elapsed);
}

int LoopCursor::resolveNext (const LoopSnapshot& s) const
{
    if (s.count <= 0)
        return -1;
    if (ghost)
    {
        if (! nextFirst)
        {
            const int index = indexOf (s, nextId);
            if (index >= 0)
                return index;
        }
        return 0;
    }
    const int index = indexOf (s, curId);
    return index < 0 ? 0 : (index + 1) % s.count;
}

LoopPlan LoopCursor::start (const LoopSnapshot& s, double hostBeat, double beatsPerSample, int numSamples)
{
    auto plan = planLoop (s, hostBeat, beatsPerSample, numSamples);
    const double bufferBeats = numSamples * beatsPerSample;

    if (plan.startIndex < 0 && plan.numChanges == 0)
    {
        mode = hostBeat < 0.0 ? Mode::PreRoll : Mode::Idle; // waiting for beat 0 of the host
        return plan;
    }

    // Where the cursor is at the end of this block of audio
    int lastIndex = plan.startIndex;
    double lastStartRelative = -plan.startElapsed;
    if (plan.numChanges > 0)
    {
        lastIndex = plan.changes[(size_t) (plan.numChanges - 1)].index;
        lastStartRelative = plan.changes[(size_t) (plan.numChanges - 1)].startBeat - hostBeat;
    }
    mode = Mode::Running;
    ghost = false;
    nextFirst = true;
    nextId = 0;
    curId = s.blocks[(size_t) lastIndex].id;
    length = s.blocks[(size_t) lastIndex].beats;
    elapsed = bufferBeats - lastStartRelative;
    expectedHost = hostBeat + bufferBeats;
    return plan;
}

LoopPlan LoopCursor::plan (const LoopSnapshot& s, double hostBeat, bool resync, double beatsPerSample, int numSamples)
{
    LoopPlan plan;
    if (beatsPerSample <= 0.0 || numSamples <= 0 || ! std::isfinite (hostBeat))
    {
        reset();
        return plan;
    }

    const bool emptied = s.count <= 0 || s.total <= 0.0;
    if (emptied && (mode != Mode::Running || ! ghost || resync))
    {
        reset(); // nothing to play (a block that was deleted when the bar was emptied still plays to its end)
        return plan;
    }
    if (! emptied && (mode != Mode::Running || resync))
        return start (s, hostBeat, beatsPerSample, numSamples);

    // Going on: the cursor counts with the host's time. A small difference from where the host should be is taken in.
    const double bufferBeats = numSamples * beatsPerSample;
    elapsed = std::max (0.0, elapsed + (hostBeat - expectedHost));

    int index = ghost ? -1 : indexOf (s, curId);
    if (! ghost && index < 0 && ! emptied)
        ghost = true; // the bar changed without the cursor being told (should not happen): the block plays to its end

    plan.playing = true;
    plan.startIndex = ghost ? -1 : index;
    plan.startElapsed = elapsed;
    plan.startBlockBeat = hostBeat - elapsed;
    plan.startBlockBeats = length;
    plan.startsOnLine = false; // a block line that falls on the first sample is the first change of the plan

    double blockStart = -elapsed; // relative to the first sample of this block of audio, in beats
    double blockEnd = blockStart + length;
    while (plan.numChanges < kMaxLoopChanges)
    {
        const int sample = std::max (0, (int) std::ceil (blockEnd / beatsPerSample - kSampleTolerance));
        if (sample >= numSamples)
            break;

        const int next = resolveNext (s);
        if (next < 0) // the bar was emptied and the block has played to its end
        {
            plan.changes[(size_t) plan.numChanges++] = { sample, -1, hostBeat + blockEnd };
            reset();
            return plan;
        }

        plan.changes[(size_t) plan.numChanges++] = { sample, next, hostBeat + blockEnd };
        curId = s.blocks[(size_t) next].id;
        length = s.blocks[(size_t) next].beats;
        ghost = false;
        nextFirst = true;
        nextId = 0;
        blockStart = blockEnd;
        blockEnd += length;
    }

    elapsed = bufferBeats - blockStart;
    expectedHost = hostBeat + bufferBeats;
    return plan;
}
}
