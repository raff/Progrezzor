// Copyright (C) 2026 Henrik Juhl. Part of Progrezzor, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <vector>

#include "ChordEngine.h"
#include "Progression.h"

// Learn mode: the user plays the chords of the progression one after the other, on the keys of a MIDI keyboard. The session
// knows which block is next, looks at which keys are held, counts the wrong tries, and says when the whole progression has been
// played. It waits for the user: there is no tempo. Pure C++, no JUCE.
//
// A chord is right when the keys held are exactly the notes of the chord, in any octave and in any order (so any inversion and
// any doubling is right), and all of them are held at the same time. Other notes held make it wrong.
namespace engine
{
constexpr int kLearnHintAfterTries = 2;      // after this many wrong tries the hint is shown, whatever the settings say
constexpr double kLearnMismatchMs = 450.0;   // enough keys held for a chord, but not the right ones, for this long: a wrong try

// The twelve pitch classes of a chord in a key, as bits (bit 0 is C)
inline unsigned chordPitchClasses (const Chord& chord, int tonic)
{
    const auto& shape = shapeOf (chord.type);
    unsigned classes = 0;
    for (int i = 0; i < shape.count; ++i)
        classes |= 1u << wrap12 (tonic + chord.root + shape.intervals[i]);
    return classes;
}

class LearnSession
{
public:
    enum class Outcome { Nothing, Right, Wrong, Finished };

    void start (const Progression& progression)
    {
        total = (int) progression.size();
        played.assign (progression.size(), false);
        wrongTries = 0;
        current = total > 0 ? 0 : -1;
        isFinished = false;
        isStarted = total > 0;
        resetAttempt();
        waitForRelease = false;
    }

    bool started() const { return isStarted; }
    int target() const { return current; }               // the block to play now, -1 if there is none
    int tries() const { return wrongTries; }             // wrong tries on this block
    bool finished() const { return isFinished; }
    bool isPlayed (int block) const { return block >= 0 && block < (int) played.size() && played[(size_t) block]; }
    int blockCount() const { return total; }
    int playedCount() const { int n = 0; for (bool p : played) n += p ? 1 : 0; return n; }
    bool needsHint() const { return wrongTries >= kLearnHintAfterTries; }

    // Called often (every tick of the editor) with the keys that are held now: `classes` are their pitch classes (bit 0 is C) and
    // `count` is how many keys. `nowMs` is any clock that counts milliseconds.
    Outcome update (unsigned classes, int count, double nowMs, const Progression& progression, int tonic)
    {
        if (! isStarted || isFinished || current < 0 || current >= (int) progression.size())
            return Outcome::Nothing;

        // After a right chord the keys must come up before the next one is looked at
        if (waitForRelease)
        {
            if (count == 0)
                waitForRelease = false;
            return Outcome::Nothing;
        }

        if (count == 0)
        {
            // Everything let go: if something was played and it never was right, that was a try
            const bool wasWrong = attempting && ! reportedWrong && maxHeld >= 1;
            resetAttempt();
            if (wasWrong)
            {
                ++wrongTries;
                return Outcome::Wrong;
            }
            return Outcome::Nothing;
        }

        attempting = true;
        if (count > maxHeld)
            maxHeld = count;

        const auto& chord = progression[(size_t) current].chord;
        const unsigned wanted = chordPitchClasses (chord, tonic);
        const int needed = shapeOf (chord.type).count;

        if (classes == wanted && count >= needed)
        {
            played[(size_t) current] = true;
            wrongTries = 0;
            ++current;
            resetAttempt();
            waitForRelease = true;
            if (current >= total)
            {
                current = -1;
                isFinished = true;
                return Outcome::Finished;
            }
            return Outcome::Right;
        }

        // Enough keys for a chord, but not the right ones, and they stay: a wrong try, once
        if (count >= needed && ! reportedWrong)
        {
            if (mismatchSince < 0.0)
                mismatchSince = nowMs;
            else if (nowMs - mismatchSince >= kLearnMismatchMs)
            {
                reportedWrong = true;
                ++wrongTries;
                return Outcome::Wrong;
            }
        }
        else if (classes == wanted || count < needed)
            mismatchSince = -1.0;

        return Outcome::Nothing;
    }

private:
    void resetAttempt()
    {
        attempting = false;
        reportedWrong = false;
        mismatchSince = -1.0;
        maxHeld = 0;
    }

    std::vector<bool> played;
    int total = 0;
    int current = -1;
    int wrongTries = 0;
    bool isFinished = false;
    bool isStarted = false;
    bool attempting = false;
    bool reportedWrong = false;
    bool waitForRelease = false;
    double mismatchSince = -1.0;
    int maxHeld = 0;
};
}
