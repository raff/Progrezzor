// Copyright (C) 2026 Henrik Juhl. Part of Progrezzor, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

// Keys, scales and noteNumber(). Pure C++, no JUCE. Adapted from SeqVerse's Music.h (without the arpeggio and rhythm parts).
namespace engine
{
constexpr int kNumKeys = 12;

// Octaves are numbered so that C4 is MIDI note 60.
constexpr int noteNumber (int key, int octave, int interval) { return 12 * (octave + 1) + key + interval; }

// What a saved project stores is the index, so the order can only grow at the end.
enum class ScaleId { Minor, Major, Dorian, Phrygian };
constexpr int kNumScales = 4;

struct Scale
{
    const char* name;
    int intervals[7]; // semitones above the tonic, ascending, all within one octave
    int relativeMajorOffset; // semitones from the tonic up to the major key with the same notes (decides b or # in names)
};

constexpr Scale scales[kNumScales] = {
    { "Minor", { 0, 2, 3, 5, 7, 8, 10 }, 3 },
    { "Major", { 0, 2, 4, 5, 7, 9, 11 }, 0 },
    { "Dorian", { 0, 2, 3, 5, 7, 9, 10 }, 10 },
    { "Phrygian", { 0, 1, 3, 5, 7, 8, 10 }, 8 },
};

constexpr const Scale& scaleOf (ScaleId id) { return scales[(int) id]; }

constexpr int kScaleSize = 7;

// A key is a tonic (0 = C ... 11 = B) and a scale.
struct Key
{
    int tonic = 9; // A
    ScaleId scale = ScaleId::Minor; // A Minor is the default
};

constexpr int wrap12 (int semitones) { return ((semitones % 12) + 12) % 12; }
}
