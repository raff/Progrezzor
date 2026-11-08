// Copyright (C) 2026 Henrik Juhl. Part of Progrezzor, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

#include "Arp.h"

// Chord rhythm: the whole chord is struck in a rhythmic pattern (the Rhythm choice of the arp panel). Pure C++, no JUCE, no
// allocation: it runs on the audio thread and in the MIDI export, and both give the same strikes at the same times.
//
// A pattern is one cycle of 4 beats. At speed 1/8 the cycle has 8 slots (an eighth note each), at 1/16 it has 16 (a sixteenth
// each); every pattern is written out by hand for both. The pattern starts over at the start of each block, and the slots
// follow the host's grid like the arp does. There is no randomness. The first slot of the cycle is accented a little.
// The Euclid pattern is the arp's rhythm engine (Hits and Rotation).
namespace engine
{
constexpr int kNumRhythmPatterns = 12;
enum class RhythmPattern { Offbeat, Stab, DoubleStab, Skank, Eighths, Push, Gallop, Charleston, Tresillo, Clave, Pulse, Euclid };
constexpr const char* rhythmPatternNames[kNumRhythmPatterns] = { "Offbeat", "Stab", "Double stab", "Skank", "Eighths", "Push",
                                                                  "Gallop", "Charleston", "Tresillo", "Clave", "Pulse", "Euclid" };
constexpr int kNumRhythmSpeeds = 2;
constexpr const char* rhythmSpeedNames[kNumRhythmSpeeds] = { "1/8", "1/16" };
constexpr int kNumRhythmLengths = 3;
constexpr const char* rhythmLengthNames[kNumRhythmLengths] = { "Short", "Medium", "Long" };

constexpr double kRhythmCycleBeats = 4.0;
constexpr double kRhythmAccentOff = 0.82; // the strikes that are not the first of the cycle have this much of the velocity

// 'x' is a strike. [0] is for 1/8 (8 slots), [1] for 1/16 (16 slots). Euclid is made, see below.
struct RhythmShape
{
    const char* eighths;
    const char* sixteenths;
};

constexpr RhythmShape rhythmShapes[kNumRhythmPatterns] = {
    { ".x.x.x.x", "..x...x...x...x." }, // Offbeat: between the beats
    { "x.......", "x..............." }, // Stab: one strike on the first beat
    { "x.x.....", "x...x..........." }, // Double stab: beats 1 and 2
    { "..x...x.", "....x.......x..." }, // Skank: beats 2 and 4
    { "xxxxxxxx", "x.x.x.x.x.x.x.x." }, // Eighths: every eighth (at 1/16 every other sixteenth, so the same)
    { "x...x..x", "x.......x.....x." }, // Push: 1, 3 and a push into the next bar
    { "x.xx.xx.", "x.xxx.xxx.xxx.xx" }, // Gallop: long, short, short
    { "x..x....", "x.....x........." }, // Charleston: beat 1 and the "and" of 2
    { "x..x..x.", "x.....x.....x..." }, // Tresillo: 3 + 3 + 2
    { "x..x.x.x", "x..x..x...x.x..." }, // Clave: three strikes, then two
    { "x.x.x.x.", "x...x...x...x..." }, // Pulse: every beat
    { "........", "................" }, // Euclid: made from Hits and Rotation
};

struct RhythmSettings
{
    bool on = false;
    int pattern = 0;  // RhythmPattern
    int speed = 0;    // 0: 1/8, 1: 1/16
    int length = 1;   // 0 Short, 1 Medium, 2 Long
    int hits = 8;     // Euclid only: how many of the slots play
    int rotation = 0; // Euclid only: moved later by this many slots

    int slots() const { return speed == 0 ? 8 : 16; }
    double slotBeats() const { return kRhythmCycleBeats / (double) slots(); }
    int patternIndex() const { return std::clamp (pattern, 0, kNumRhythmPatterns - 1); }

    // Is there a strike at this slot? `step` counts slots from the start of the block.
    bool isHit (std::int64_t step) const
    {
        const int position = (int) (((step % slots()) + slots()) % slots());
        if (patternIndex() == (int) RhythmPattern::Euclid)
            return EuclidPattern (slots(), std::min (hits, slots()), rotation).isOnset (position);
        const auto& shape = rhythmShapes[patternIndex()];
        return (speed == 0 ? shape.eighths : shape.sixteenths)[position] == 'x';
    }

    // How long the chord rings, in beats. Short and Medium are a part of the slot; Long lasts to the next strike.
    double noteBeats (std::int64_t step) const
    {
        if (length <= 0)
            return 0.3 * slotBeats();
        if (length == 1)
            return 0.8 * slotBeats();
        int distance = 1;
        while (distance < slots() && ! isHit (step + distance))
            ++distance;
        return 0.92 * (double) distance * slotBeats();
    }

    // The first slot of the cycle is the accent
    int velocityFor (int base, std::int64_t step) const
    {
        const int position = (int) (((step % slots()) + slots()) % slots());
        return position == 0 ? base : std::max (1, (int) (base * kRhythmAccentOff + 0.5));
    }
};
}
