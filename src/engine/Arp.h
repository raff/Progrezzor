// Copyright (C) 2026 Henrik Juhl. Part of Progrezzor, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <array>
#include <cstdint>

#include "ChordEngine.h"

// The arpeggio: a chord played one note at a time instead of all at once. Pure C++, no JUCE, no allocation: it runs on the
// audio thread and in the MIDI export, and both give the same notes at the same times. Adapted from SeqVerse (Music.h).
//
// When Arp is on, everything is played this way: the pads, the keys, the blocks and the progression. A chord is cut into
// steps (one every `rate`), a rhythm (a Euclidean pattern: `hits` spread evenly over `steps`) says which steps play, and the
// style says which note of the chord each hit plays. Everything is decided by the step number alone, so the same chord with
// the same settings gives the same notes every time, also the random style.
namespace engine
{
enum class ArpStyle { Up, Down, UpDown, Random };
constexpr int kNumArpStyles = 4;

// How far apart the steps are, in beats: slow, normal, fast, very fast (1, 2, 4 and 8 notes in a beat)
constexpr int kNumArpRates = 4;
constexpr double arpRateBeats[kNumArpRates] = { 1.0, 0.5, 0.25, 0.125 };
constexpr int kDefaultArpRate = 1;

constexpr int kMaxArpOctaves = 2;
constexpr int kMinArpSteps = 4;
constexpr int kMaxArpSteps = 16;
constexpr double kArpGate = 0.8; // a note sounds for this much of a step
constexpr int kArpRandomSeed = 7;

// A Euclidean rhythm: `pulses` onsets spread as evenly as possible over `steps` steps (Bjorklund's algorithm), so E(3,8) is
// x..x..x. and E(5,8) is x.xx.xx. `rotation` moves it later by that many steps. Pulses above the steps play every step.
struct EuclidPattern
{
    int steps = 8;
    int pulses = 8;
    std::array<bool, kMaxArpSteps> onset {};
    std::array<int, kMaxArpSteps + 1> pulsesBefore {};

    EuclidPattern() : EuclidPattern (8, 8, 0) {}

    EuclidPattern (int stepsIn, int pulsesIn, int rotation)
        : steps (stepsIn < 1 ? 1 : stepsIn > kMaxArpSteps ? kMaxArpSteps : stepsIn), pulses (0)
    {
        pulses = pulsesIn < 0 ? 0 : pulsesIn > steps ? steps : pulsesIn;

        std::array<bool, kMaxArpSteps> base {};
        bjorklund (base);

        const int shift = ((rotation % steps) + steps) % steps;
        for (int i = 0; i < steps; ++i)
            onset[(size_t) ((i + shift) % steps)] = base[(size_t) i];

        int count = 0;
        for (int i = 0; i < steps; ++i)
        {
            pulsesBefore[(size_t) i] = count;
            count += onset[(size_t) i] ? 1 : 0;
        }
        pulsesBefore[(size_t) steps] = count;
    }

    int position (std::int64_t step) const { return (int) (((step % steps) + steps) % steps); }
    bool isOnset (std::int64_t step) const { return onset[(size_t) position (step)]; }

    // How many onsets came before this step, counting from step 0: the number of this onset if it is one
    std::int64_t pulseNumber (std::int64_t step) const
    {
        const int p = position (step);
        return (step - p) / steps * pulses + pulsesBefore[(size_t) p];
    }

private:
    struct Builder
    {
        const int* counts;
        const int* remainders;
        int* pattern;
        int length;

        void build (int level)
        {
            if (level == -1)
                pattern[length++] = 0;
            else if (level == -2)
                pattern[length++] = 1;
            else
            {
                for (int i = 0; i < counts[level]; ++i)
                    build (level - 1);
                if (remainders[level] != 0)
                    build (level - 2);
            }
        }
    };

    // Bjorklund's algorithm, the same as in SeqVerse
    void bjorklund (std::array<bool, kMaxArpSteps>& out) const
    {
        if (pulses == 0)
            return;
        if (pulses == steps)
        {
            for (int i = 0; i < steps; ++i)
                out[(size_t) i] = true;
            return;
        }

        int counts[kMaxArpSteps + 2] = {};
        int remainders[kMaxArpSteps + 2] = {};
        int divisor = steps - pulses;
        remainders[0] = pulses;

        int level = 0;
        do
        {
            counts[level] = divisor / remainders[level];
            remainders[level + 1] = divisor % remainders[level];
            divisor = remainders[level];
            ++level;
        } while (remainders[level] > 1);
        counts[level] = divisor;

        int pattern[kMaxArpSteps] = {};
        Builder builder { counts, remainders, pattern, 0 };
        builder.build (level);

        int first = 0;
        while (first < steps - 1 && pattern[first] == 0)
            ++first;
        for (int i = 0; i < steps; ++i)
            out[(size_t) i] = pattern[(i + first) % steps] != 0;
    }
};

// The notes an arp can play: the chord's notes, and with two octaves the same notes an octave higher. Lowest first.
struct ArpPool
{
    std::array<int, kMaxChordNotes * kMaxArpOctaves> notes {};
    int count = 0;
};

struct ArpSettings
{
    bool on = false;
    int style = 0;       // ArpStyle
    int rate = kDefaultArpRate;
    int octaves = 1;     // 1 or 2
    int steps = 8;       // the length of the rhythm
    int hits = 8;        // how many of the steps play (all of them when it is as many as the steps)
    int rotation = 0;

    EuclidPattern pattern() const { return EuclidPattern (steps, hits, rotation); }
    double stepBeats() const { return arpRateBeats[rate < 0 ? 0 : rate >= kNumArpRates ? kNumArpRates - 1 : rate]; }
};

inline ArpPool arpPool (const Voicing& chord, int octaves)
{
    ArpPool pool;
    const int rounds = octaves < 1 ? 1 : octaves > kMaxArpOctaves ? kMaxArpOctaves : octaves;
    for (int round = 0; round < rounds; ++round)
        for (int i = 0; i < chord.count; ++i)
        {
            const int note = chord.notes[(size_t) i] + 12 * round;
            if (note >= 0 && note <= 127)
                pool.notes[(size_t) pool.count++] = note;
        }
    return pool;
}

struct ArpStepResult
{
    bool play = false;
    int note = 0;
};

// A small random number generator (splitmix64): the numbers of a step depend only on the seed and the step number
struct ArpRandom
{
    std::uint64_t state;

    static constexpr ArpRandom forStep (int seed, std::int64_t step)
    {
        return { (std::uint64_t) seed * (std::uint64_t) 0x9e3779b97f4a7c15ULL ^ (std::uint64_t) step * (std::uint64_t) 0xd1342543de82ef95ULL };
    }

    constexpr std::uint64_t next()
    {
        state += (std::uint64_t) 0x9e3779b97f4a7c15ULL;
        std::uint64_t z = state;
        z = (z ^ (z >> 30)) * (std::uint64_t) 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * (std::uint64_t) 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }

    constexpr int nextInt (int n) { return (int) ((next() >> 33) % (std::uint64_t) n); }
};

// Which of `size` notes (0 is the lowest) the hit with number `pulse` plays
inline int arpIndex (ArpStyle style, int size, std::int64_t pulse)
{
    if (size <= 1)
        return 0;
    const auto cycle = [&] (std::int64_t length) { return (int) (((pulse % length) + length) % length); };
    switch (style)
    {
        case ArpStyle::Down:   return size - 1 - cycle (size);
        case ArpStyle::UpDown: { const int pos = cycle (2 * size - 2); return pos < size ? pos : 2 * size - 2 - pos; } // 0 1 2 1 0 1 2 ...
        case ArpStyle::Random: return ArpRandom::forStep (kArpRandomSeed, pulse).nextInt (size);
        case ArpStyle::Up:
        default:               return cycle (size);
    }
}

// What happens at step number `step` of a chord (counted from the moment the chord starts): a note, or a rest
inline ArpStepResult arpStep (const ArpSettings& settings, const EuclidPattern& pattern, const ArpPool& pool, std::int64_t step)
{
    ArpStepResult result;
    if (pool.count == 0 || ! pattern.isOnset (step))
        return result;
    const int style = settings.style < 0 ? 0 : settings.style >= kNumArpStyles ? kNumArpStyles - 1 : settings.style;
    result.play = true;
    result.note = pool.notes[(size_t) arpIndex ((ArpStyle) style, pool.count, pattern.pulseNumber (step))];
    return result;
}
}
