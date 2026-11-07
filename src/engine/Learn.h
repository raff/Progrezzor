// Copyright (C) 2026 Henrik Juhl. Part of Progrezzor, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <vector>

#include "Progression.h"

// Learn mode: the user plays the chords of the progression one after the other. The session knows which block is next, counts
// the wrong tries, and says when the whole progression has been played. It waits for the user: there is no tempo. Pure C++,
// no JUCE.
//
// A chord is "right" when it has the same root and the same kind as the block's chord (an inversion or an octave does not
// matter). A block that no pad can play is skipped.
namespace engine
{
constexpr int kLearnHintAfterTries = 2; // after this many wrong tries the hint is shown, whatever the settings say

inline bool sameChordForLearning (const Chord& a, const Chord& b)
{
    return wrap12 (a.root) == wrap12 (b.root) && a.type == b.type;
}

class LearnSession
{
public:
    enum class Outcome { Nothing, Right, Wrong, Finished };

    // `playable[i]` is false for a block that no pad can play: it is skipped and counts as done
    void start (const Progression& progression, const std::vector<bool>& playable)
    {
        done.assign (progression.size(), false);
        skipped.assign (progression.size(), false);
        for (size_t i = 0; i < progression.size(); ++i)
            if (i < playable.size() && ! playable[i])
            {
                done[i] = true;
                skipped[i] = true;
            }
        wrongTries = 0;
        current = nextUndone (0);
        isFinished = ! progression.empty() && current < 0;
        isStarted = ! progression.empty();
    }

    bool started() const { return isStarted; }
    int target() const { return current; }                // the block to play now, -1 if there is none
    int tries() const { return wrongTries; }              // wrong tries on this block
    bool finished() const { return isFinished; }
    bool isDone (int block) const { return block >= 0 && block < (int) done.size() && done[(size_t) block]; }
    int total() const { return (int) done.size(); }
    int doneCount() const { int n = 0; for (bool d : done) n += d ? 1 : 0; return n; }
    bool isSkipped (int block) const { return block >= 0 && block < (int) skipped.size() && skipped[(size_t) block]; }
    bool isPlayed (int block) const { return isDone (block) && ! isSkipped (block); } // played by the user, not skipped
    int playableCount() const { int n = 0; for (bool s : skipped) n += s ? 0 : 1; return n; }
    int playedCount() const { return doneCount() - (total() - playableCount()); }
    bool needsHint() const { return wrongTries >= kLearnHintAfterTries; }

    // A chord was played. Says what that was.
    Outcome play (const Chord& played, const Progression& progression)
    {
        if (! isStarted || isFinished || current < 0 || current >= (int) progression.size())
            return Outcome::Nothing;

        if (! sameChordForLearning (played, progression[(size_t) current].chord))
        {
            ++wrongTries;
            return Outcome::Wrong;
        }

        done[(size_t) current] = true;
        wrongTries = 0;
        current = nextUndone (current + 1);
        if (current < 0)
        {
            isFinished = true;
            return Outcome::Finished;
        }
        return Outcome::Right;
    }

private:
    int nextUndone (int from) const
    {
        for (int i = from; i < (int) done.size(); ++i)
            if (! done[(size_t) i])
                return i;
        return -1;
    }

    std::vector<bool> done, skipped;
    int current = -1;
    int wrongTries = 0;
    bool isFinished = false;
    bool isStarted = false;
};
}
