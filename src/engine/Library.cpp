// Copyright (C) 2026 Henrik Juhl. Part of Progrezzor, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only
#include "Library.h"

namespace engine
{
namespace
{
constexpr int kMaxSteps = 8;

struct Step
{
    int root;       // semitones from the tonic
    ChordType type;
    double beats;   // 4 is one bar
    int inversion = kAutoInversion; // 1: the third in the bass (G/B), 2: the fifth in the bass (C/G)
};

struct Entry
{
    int count;
    Step steps[kMaxSteps];
};

constexpr ChordType Mi = ChordType::Minor;
constexpr ChordType Ma = ChordType::Major;
constexpr ChordType M7 = ChordType::Min7;
constexpr ChordType X7 = ChordType::Maj7;
constexpr ChordType A9 = ChordType::Add9;
constexpr ChordType D7 = ChordType::Dom7;
constexpr ChordType N9 = ChordType::Dom9;
constexpr ChordType M9 = ChordType::Min9;
constexpr ChordType S2 = ChordType::Sus2;
constexpr ChordType S4 = ChordType::Sus4;
constexpr ChordType P5 = ChordType::Power5;

// Inversions in the lists below: the last number of a step. 1 = first inversion (G/B), 2 = second (C/G).

// ---- Melancholic, minor ------------------------------------------------------------------------------------------
// The list for Minor, Dorian and Phrygian. Each progression is used in the scales whose notes it fits, see entryFits.
// Sad and beautiful: minor chords, minor sevenths, an added ninth or a major seventh where it softens. The roots are
// written as semitones from the tonic of a natural minor scale: 0 is the tonic, 3 the third, 5 the fourth, 7 the fifth,
// 8 the sixth, 10 the seventh.
constexpr Entry melancholicMinor[] = {
    { 4, { { 0, Mi, 4 }, { 8, Ma, 4 }, { 3, Ma, 4 }, { 10, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 10, Ma, 4 }, { 8, Ma, 4 }, { 10, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 5, Mi, 4 }, { 10, Ma, 4 }, { 3, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 8, Ma, 4 }, { 5, Mi, 4 }, { 0, Mi, 4 } } },
    { 4, { { 0, Mi, 4 }, { 3, Ma, 4 }, { 10, Ma, 4 }, { 5, Mi, 4 } } },
    { 4, { { 0, Mi, 4 }, { 7, Mi, 4 }, { 8, Ma, 4 }, { 5, Mi, 4 } } },
    { 4, { { 0, M7, 4 }, { 8, X7, 4 }, { 3, Ma, 4 }, { 10, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 5, Mi, 4 }, { 0, Mi, 4 }, { 10, Ma, 4 } } },
    { 3, { { 8, Ma, 4 }, { 10, Ma, 4 }, { 0, Mi, 8 } } },
    { 4, { { 0, Mi, 4 }, { 3, Ma, 4 }, { 5, Mi, 4 }, { 8, Ma, 4 } } },
    { 4, { { 5, Mi, 4 }, { 8, Ma, 4 }, { 3, Ma, 4 }, { 10, Ma, 4 } } },
    { 4, { { 0, M7, 4 }, { 8, X7, 4 }, { 3, A9, 4 }, { 10, A9, 4 } } },
    { 4, { { 0, Mi, 4 }, { 10, Ma, 4 }, { 8, Ma, 4 }, { 7, Mi, 4 } } },
    { 4, { { 0, Mi, 4 }, { 3, Ma, 4 }, { 8, Ma, 4 }, { 10, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 5, M7, 4 }, { 10, Ma, 4 }, { 3, X7, 4 } } },
    { 4, { { 0, M7, 4 }, { 5, M7, 4 }, { 8, X7, 4 }, { 10, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 8, Ma, 4 }, { 0, Mi, 4 }, { 10, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 7, Mi, 4 }, { 5, Mi, 4 }, { 0, Mi, 4 } } },
    { 4, { { 8, Ma, 4 }, { 3, Ma, 4 }, { 10, Ma, 4 }, { 0, Mi, 4 } } },
    { 2, { { 0, Mi, 8 }, { 8, Ma, 8 } } },
    { 3, { { 0, M7, 8 }, { 10, Ma, 4 }, { 8, X7, 4 } } },
    { 6, { { 0, Mi, 4 }, { 8, Ma, 4 }, { 3, Ma, 4 }, { 10, Ma, 4 }, { 5, Mi, 4 }, { 0, Mi, 4 } } },

    // Fit both Minor and Dorian: only the tonic, the third, the minor fifth and the seventh
    { 4, { { 0, Mi, 4 }, { 3, Ma, 4 }, { 10, Ma, 4 }, { 7, Mi, 4 } } },
    { 4, { { 0, Mi, 4 }, { 7, Mi, 4 }, { 3, Ma, 4 }, { 10, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 10, Ma, 4 }, { 3, Ma, 4 }, { 7, Mi, 4 } } },

    // Fit Dorian (a major chord on the fourth and a minor chord on the second are its colours)
    { 4, { { 0, Mi, 4 }, { 5, Ma, 4 }, { 3, Ma, 4 }, { 10, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 10, Ma, 4 }, { 5, Ma, 4 }, { 0, Mi, 4 } } },
    { 4, { { 0, Mi, 4 }, { 2, Mi, 4 }, { 3, Ma, 4 }, { 5, Ma, 4 } } },
    { 4, { { 0, M7, 4 }, { 5, Ma, 4 }, { 10, Ma, 4 }, { 3, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 7, Mi, 4 }, { 5, Ma, 4 }, { 10, Ma, 4 } } },
    { 3, { { 3, Ma, 4 }, { 5, Ma, 4 }, { 0, Mi, 8 } } },
    { 4, { { 0, Mi, 4 }, { 5, A9, 4 }, { 3, Ma, 4 }, { 10, A9, 4 } } },

    // Fit Phrygian (the chord a semitone above the tonic is its colour)
    { 4, { { 0, Mi, 4 }, { 1, Ma, 4 }, { 0, Mi, 4 }, { 3, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 8, Ma, 4 }, { 1, Ma, 4 }, { 0, Mi, 4 } } },
    { 4, { { 0, Mi, 4 }, { 1, Ma, 4 }, { 3, Ma, 4 }, { 10, Mi, 4 } } },
    { 4, { { 0, Mi, 4 }, { 5, Mi, 4 }, { 1, Ma, 4 }, { 0, Mi, 4 } } },

    // With a bass line that walks (an inversion in the bass)
    { 4, { { 0, Mi, 4 }, { 7, Mi, 4, 1 }, { 8, Ma, 4 }, { 10, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 7, Mi, 4, 1 }, { 8, Ma, 4 }, { 5, Mi, 4 } } },
    { 4, { { 5, Mi, 4 }, { 0, Mi, 4, 1 }, { 8, Ma, 4 }, { 10, Ma, 4 } } },
};

// ---- Melancholic, major ------------------------------------------------------------------------------------------
// Wistful, in a major key: the minor chords (2, 4 and 9 semitones up) carry the feeling. The roots are semitones from
// the tonic of a major scale: 2 the second, 4 the third, 5 the fourth, 7 the fifth, 9 the sixth.
constexpr Entry melancholicMajor[] = {
    { 4, { { 9, Mi, 4 }, { 5, Ma, 4 }, { 0, Ma, 4 }, { 7, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 4, Mi, 4 }, { 9, Mi, 4 }, { 5, Ma, 4 } } },
    { 4, { { 9, Mi, 4 }, { 2, Mi, 4 }, { 5, Ma, 4 }, { 0, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 9, Mi, 4 }, { 2, Mi, 4 }, { 7, Ma, 4 } } },
    { 4, { { 5, Ma, 4 }, { 0, Ma, 4 }, { 9, Mi, 4 }, { 7, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 7, Ma, 4 }, { 9, Mi, 4 }, { 4, Mi, 4 } } },
    { 4, { { 9, Mi, 4 }, { 5, Ma, 4 }, { 7, Ma, 4 }, { 0, Ma, 4 } } },
    { 4, { { 0, X7, 4 }, { 9, M7, 4 }, { 2, M7, 4 }, { 7, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 5, Ma, 4 }, { 9, Mi, 4 }, { 7, Ma, 4 } } },
    { 4, { { 9, Mi, 4 }, { 7, Ma, 4 }, { 5, Ma, 4 }, { 0, Ma, 4 } } },
    { 4, { { 5, X7, 4 }, { 4, M7, 4 }, { 9, M7, 4 }, { 7, Ma, 4 } } },
    { 4, { { 9, Mi, 4 }, { 4, Mi, 4 }, { 5, Ma, 4 }, { 0, Ma, 4 } } },
    { 4, { { 0, A9, 4 }, { 9, M7, 4 }, { 5, X7, 4 }, { 7, Ma, 4 } } },
    { 4, { { 2, Mi, 4 }, { 9, Mi, 4 }, { 5, Ma, 4 }, { 0, Ma, 4 } } },
    { 2, { { 9, Mi, 8 }, { 5, Ma, 8 } } },
    { 3, { { 0, Ma, 4 }, { 4, Mi, 4 }, { 9, Mi, 8 } } },
{ 4, { { 9, Mi, 4 }, { 0, Ma, 4 }, { 5, Ma, 4 }, { 7, Ma, 4 } } },
    { 4, { { 4, Mi, 4 }, { 5, Ma, 4 }, { 9, Mi, 4 }, { 7, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 9, Mi, 4 }, { 5, Ma, 4 }, { 4, Mi, 4 } } },
    { 3, { { 5, Ma, 4 }, { 4, Mi, 4 }, { 9, Mi, 8 } } },

    // With a bass line that walks (an inversion in the bass)
    { 4, { { 9, Mi, 4 }, { 4, Mi, 4, 1 }, { 5, Ma, 4 }, { 7, Ma, 4 } } },
    { 4, { { 5, Ma, 4 }, { 0, Ma, 4, 1 }, { 2, Mi, 4 }, { 7, Ma, 4, 1 } } },
};

struct List
{
    const Entry* entries;
    int count;
};

template <size_t N>
constexpr List listOf (const Entry (&entries)[N]) { return { entries, (int) N }; }


// The libraries of the other moods follow the same rules: one list for minor (used by Minor, Dorian and Phrygian, each
// entry only where all its chords fit) and one for major. Chords with more colour (sevenths, ninths, sus, power chords)
// are written in too; Suggest only picks progressions whose chords are all pads, so for now it is the plain chords that
// are suggested and the others wait for the day the rule is relaxed.

// ---- Bright: happy and open, clean triads ------------------------------------------------------------------------
constexpr Entry brightMinor[] = {
    { 4, { { 3, Ma, 4 }, { 10, Ma, 4 }, { 0, Mi, 4 }, { 8, Ma, 4 } } },
    { 4, { { 8, Ma, 4 }, { 3, Ma, 4 }, { 10, Ma, 4 }, { 10, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 3, Ma, 4 }, { 10, Ma, 4 }, { 8, Ma, 4 } } },
    { 4, { { 3, Ma, 4 }, { 8, Ma, 4 }, { 10, Ma, 4 }, { 0, Mi, 4 } } },
    { 4, { { 8, Ma, 4 }, { 10, Ma, 4 }, { 0, Mi, 4 }, { 3, Ma, 4 } } },
    { 4, { { 10, Ma, 4 }, { 8, Ma, 4 }, { 3, Ma, 4 }, { 0, Mi, 4 } } },
    { 4, { { 3, Ma, 4 }, { 3, Ma, 4 }, { 8, Ma, 4 }, { 10, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 8, Ma, 4 }, { 3, Ma, 4 }, { 10, Ma, 4 } } },
    { 4, { { 8, Ma, 2 }, { 10, Ma, 2 }, { 3, Ma, 4 }, { 0, Mi, 4 } } },
    { 4, { { 3, Ma, 4 }, { 10, Ma, 4 }, { 8, Ma, 4 }, { 5, Mi, 4 } } },
    { 4, { { 3, Ma, 4 }, { 10, Ma, 4 }, { 3, Ma, 4 }, { 7, Mi, 4 } } },
    { 4, { { 0, Mi, 4 }, { 10, Ma, 4 }, { 3, Ma, 4 }, { 7, Mi, 4 } } },
    { 4, { { 3, Ma, 4 }, { 7, Mi, 4 }, { 10, Ma, 4 }, { 0, Mi, 4 } } },
    { 4, { { 10, Ma, 4 }, { 3, Ma, 4 }, { 7, Mi, 4 }, { 0, Mi, 4 } } },
    { 4, { { 3, Ma, 4 }, { 5, Ma, 4 }, { 10, Ma, 4 }, { 0, Mi, 4 } } },
    { 4, { { 10, Ma, 4 }, { 5, Ma, 4 }, { 3, Ma, 4 }, { 0, Mi, 4 } } },
    { 4, { { 0, Mi, 4 }, { 5, Ma, 4 }, { 10, Ma, 4 }, { 3, Ma, 4 } } },
    { 4, { { 3, Ma, 4 }, { 8, Ma, 4 }, { 0, Mi, 4 }, { 8, Ma, 4 } } },
    { 4, { { 3, Ma, 4 }, { 0, Mi, 4 }, { 8, Ma, 4 }, { 5, Mi, 4 } } },
    { 4, { { 8, Ma, 4 }, { 3, Ma, 4 }, { 0, Mi, 4 }, { 1, Ma, 4 } } },
    { 4, { { 3, Ma, 4 }, { 8, Ma, 4 }, { 1, Ma, 4 }, { 0, Mi, 4 } } },
    { 4, { { 3, Ma, 4 }, { 0, Mi, 4 }, { 5, Mi, 4 }, { 8, Ma, 4 } } },

    // With a bass line that walks (an inversion in the bass): C G/B Am F and so on
    { 4, { { 3, Ma, 4 }, { 10, Ma, 4, 1 }, { 0, Mi, 4 }, { 8, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 10, Ma, 4, 1 }, { 3, Ma, 4 }, { 10, Ma, 4 } } },
};

constexpr Entry brightMajor[] = {
    { 4, { { 0, Ma, 4 }, { 7, Ma, 4 }, { 9, Mi, 4 }, { 5, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 5, Ma, 4 }, { 7, Ma, 4 }, { 5, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 5, Ma, 4 }, { 0, Ma, 4 }, { 7, Ma, 4 } } },
    { 4, { { 5, Ma, 4 }, { 0, Ma, 4 }, { 7, Ma, 4 }, { 0, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 7, Ma, 4 }, { 5, Ma, 4 }, { 0, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 9, Mi, 4 }, { 5, Ma, 4 }, { 7, Ma, 4 } } },
    { 4, { { 5, Ma, 4 }, { 7, Ma, 4 }, { 0, Ma, 4 }, { 0, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 2, Mi, 4 }, { 7, Ma, 4 }, { 0, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 4, Mi, 4 }, { 5, Ma, 4 }, { 7, Ma, 4 } } },
    { 4, { { 5, Ma, 4 }, { 0, Ma, 4 }, { 5, Ma, 4 }, { 7, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 7, Ma, 4 }, { 2, Mi, 4 }, { 5, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 5, Ma, 4 }, { 2, Mi, 4 }, { 7, Ma, 4 } } },
    { 4, { { 0, Ma, 2 }, { 5, Ma, 2 }, { 7, Ma, 4 }, { 0, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 7, Ma, 4 }, { 9, Mi, 4 }, { 4, Mi, 4 } } },
    { 4, { { 5, Ma, 4 }, { 5, Ma, 4 }, { 0, Ma, 4 }, { 7, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 4, Mi, 4 }, { 9, Mi, 4 }, { 7, Ma, 4 } } },
    { 4, { { 7, Ma, 4 }, { 5, Ma, 4 }, { 0, Ma, 4 }, { 0, Ma, 4 } } },
    { 3, { { 0, Ma, 4 }, { 5, Ma, 4 }, { 7, Ma, 8 } } },
    { 4, { { 0, Ma, 4 }, { 9, Mi, 4 }, { 2, Mi, 4 }, { 7, Ma, 4 } } },
    { 4, { { 5, Ma, 4 }, { 2, Mi, 4 }, { 7, Ma, 4 }, { 0, Ma, 4 } } },

    // With a bass line that walks (an inversion in the bass)
    { 4, { { 0, Ma, 4 }, { 7, Ma, 4, 1 }, { 9, Mi, 4 }, { 5, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 0, Ma, 4, 1 }, { 5, Ma, 4 }, { 7, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 7, Ma, 4, 1 }, { 5, Ma, 4, 1 }, { 7, Ma, 4 } } },
};

// ---- Warm: safe and soft, triads, add9, major sevenths -----------------------------------------------------------
constexpr Entry warmMinor[] = {
    { 4, { { 0, Mi, 4 }, { 3, Ma, 4 }, { 8, Ma, 4 }, { 10, Ma, 4 } } },
    { 4, { { 8, Ma, 4 }, { 10, Ma, 4 }, { 0, Mi, 4 }, { 0, Mi, 4 } } },
    { 4, { { 3, Ma, 4 }, { 8, Ma, 4 }, { 10, Ma, 4 }, { 3, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 8, Ma, 4 }, { 3, Ma, 4 }, { 8, Ma, 4 } } },
    { 4, { { 8, Ma, 4 }, { 3, Ma, 4 }, { 0, Mi, 4 }, { 10, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 5, Mi, 4 }, { 8, Ma, 4 }, { 3, Ma, 4 } } },
    { 4, { { 3, Ma, 4 }, { 0, Mi, 4 }, { 5, Mi, 4 }, { 8, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 10, Ma, 4 }, { 8, Ma, 4 }, { 3, Ma, 4 } } },
    { 4, { { 8, Ma, 4 }, { 5, Mi, 4 }, { 0, Mi, 4 }, { 10, Ma, 4 } } },
    { 4, { { 3, Ma, 4 }, { 8, Ma, 4 }, { 5, Mi, 4 }, { 0, Mi, 4 } } },
    { 4, { { 0, Mi, 4 }, { 3, Ma, 4 }, { 7, Mi, 4 }, { 10, Ma, 4 } } },
    { 4, { { 3, Ma, 4 }, { 10, Ma, 4 }, { 0, Mi, 4 }, { 7, Mi, 4 } } },
    { 4, { { 0, Mi, 4 }, { 7, Mi, 4 }, { 10, Ma, 4 }, { 3, Ma, 4 } } },
    { 4, { { 10, Ma, 4 }, { 3, Ma, 4 }, { 0, Mi, 4 }, { 7, Mi, 4 } } },
    { 4, { { 0, Mi, 4 }, { 5, Ma, 4 }, { 3, Ma, 4 }, { 10, Ma, 4 } } },
    { 4, { { 3, Ma, 4 }, { 5, Ma, 4 }, { 0, Mi, 4 }, { 10, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 3, Ma, 4 }, { 5, Ma, 4 }, { 3, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 3, Ma, 4 }, { 8, Ma, 4 }, { 1, Ma, 4 } } },
    { 4, { { 8, Ma, 4 }, { 3, Ma, 4 }, { 1, Ma, 4 }, { 0, Mi, 4 } } },
    { 4, { { 0, Mi, 4 }, { 8, Ma, 4 }, { 5, Mi, 4 }, { 10, Mi, 4 } } },
    { 4, { { 3, A9, 4 }, { 8, X7, 4 }, { 10, Ma, 4 }, { 0, M7, 4 } } },

    // With a bass line that walks (an inversion in the bass)
    { 4, { { 3, Ma, 4 }, { 10, Ma, 4, 1 }, { 0, Mi, 4 }, { 8, Ma, 4 } } },
    { 4, { { 8, Ma, 4 }, { 3, Ma, 4, 1 }, { 5, Mi, 4 }, { 0, Mi, 4 } } },
};

constexpr Entry warmMajor[] = {
    { 4, { { 0, Ma, 4 }, { 5, Ma, 4 }, { 9, Mi, 4 }, { 7, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 9, Mi, 4 }, { 5, Ma, 4 }, { 0, Ma, 4 } } },
    { 4, { { 5, Ma, 4 }, { 0, Ma, 4 }, { 5, Ma, 4 }, { 7, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 4, Mi, 4 }, { 5, Ma, 4 }, { 0, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 7, Ma, 4 }, { 5, Ma, 4 }, { 5, Ma, 4 } } },
    { 4, { { 5, Ma, 4 }, { 4, Mi, 4 }, { 2, Mi, 4 }, { 0, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 5, Ma, 4 }, { 0, Ma, 4 }, { 5, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 9, Mi, 4 }, { 4, Mi, 4 }, { 5, Ma, 4 } } },
    { 4, { { 5, Ma, 4 }, { 7, Ma, 4 }, { 9, Mi, 4 }, { 0, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 2, Mi, 4 }, { 5, Ma, 4 }, { 7, Ma, 4 } } },
    { 4, { { 5, Ma, 4 }, { 0, Ma, 4 }, { 9, Mi, 4 }, { 5, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 7, Ma, 4 }, { 0, Ma, 4 }, { 5, Ma, 4 } } },
    { 4, { { 9, Mi, 4 }, { 5, Ma, 4 }, { 0, Ma, 4 }, { 0, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 5, Ma, 4 }, { 4, Mi, 4 }, { 9, Mi, 4 } } },
    { 4, { { 0, Ma, 4 }, { 4, Mi, 4 }, { 2, Mi, 4 }, { 7, Ma, 4 } } },
    { 3, { { 0, Ma, 4 }, { 9, Mi, 4 }, { 5, Ma, 8 } } },
    { 4, { { 5, Ma, 4 }, { 9, Mi, 4 }, { 7, Ma, 4 }, { 0, Ma, 4 } } },
    { 4, { { 0, X7, 4 }, { 5, X7, 4 }, { 9, M7, 4 }, { 7, Ma, 4 } } },
    { 4, { { 0, A9, 4 }, { 5, X7, 4 }, { 2, M7, 4 }, { 7, D7, 4 } } },
    { 4, { { 5, X7, 4 }, { 0, A9, 4 }, { 4, M7, 4 }, { 9, M7, 4 } } },

    // With a bass line that walks (an inversion in the bass)
    { 4, { { 0, Ma, 4 }, { 7, Ma, 4, 1 }, { 9, Mi, 4 }, { 4, Mi, 4, 1 } } },
    { 4, { { 5, Ma, 4 }, { 0, Ma, 4, 1 }, { 2, Mi, 4 }, { 7, Ma, 4 } } },
};

// ---- Dreamy: floating, long chords; maj7, 9 and sus2 where the rule allows ---------------------------------------
constexpr Entry dreamyMinor[] = {
    { 3, { { 0, Mi, 8 }, { 8, Ma, 8 }, { 3, Ma, 8 } } },
    { 3, { { 8, Ma, 8 }, { 3, Ma, 8 }, { 10, Ma, 8 } } },
    { 2, { { 3, Ma, 8 }, { 8, Ma, 8 } } },
    { 3, { { 0, Mi, 8 }, { 3, Ma, 8 }, { 10, Ma, 8 } } },
    { 4, { { 8, Ma, 8 }, { 10, Ma, 8 }, { 0, Mi, 8 }, { 0, Mi, 8 } } },
    { 3, { { 3, Ma, 8 }, { 10, Ma, 8 }, { 8, Ma, 8 } } },
    { 3, { { 0, Mi, 8 }, { 5, Mi, 8 }, { 8, Ma, 8 } } },
    { 3, { { 8, Ma, 8 }, { 5, Mi, 8 }, { 0, Mi, 8 } } },
    { 4, { { 0, Mi, 4 }, { 8, Ma, 4 }, { 3, Ma, 4 }, { 10, Ma, 4 } } },
    { 3, { { 3, Ma, 8 }, { 0, Mi, 8 }, { 8, Ma, 8 } } },
    { 3, { { 0, Mi, 8 }, { 7, Mi, 8 }, { 8, Ma, 8 } } },
    { 3, { { 8, Ma, 8 }, { 3, Ma, 8 }, { 7, Mi, 8 } } },
    { 3, { { 0, Mi, 8 }, { 3, Ma, 8 }, { 7, Mi, 8 } } },
    { 3, { { 3, Ma, 8 }, { 7, Mi, 8 }, { 0, Mi, 8 } } },
    { 3, { { 0, Mi, 8 }, { 3, Ma, 8 }, { 5, Ma, 8 } } },
    { 3, { { 3, Ma, 8 }, { 5, Ma, 8 }, { 10, Ma, 8 } } },
    { 3, { { 0, Mi, 8 }, { 1, Ma, 8 }, { 3, Ma, 8 } } },
    { 3, { { 8, Ma, 8 }, { 1, Ma, 8 }, { 0, Mi, 8 } } },
    { 4, { { 0, M7, 8 }, { 8, X7, 8 }, { 3, A9, 8 }, { 10, N9, 8 } } },
    { 4, { { 0, S2, 8 }, { 3, S2, 8 }, { 8, X7, 8 }, { 10, S2, 8 } } },

    // With a bass line that walks (an inversion in the bass)
    { 3, { { 0, Mi, 8 }, { 7, Mi, 8, 1 }, { 8, Ma, 8 } } },
};

constexpr Entry dreamyMajor[] = {
    { 3, { { 0, Ma, 8 }, { 5, Ma, 8 }, { 9, Mi, 8 } } },
    { 3, { { 5, Ma, 8 }, { 0, Ma, 8 }, { 7, Ma, 8 } } },
    { 2, { { 0, Ma, 8 }, { 5, Ma, 8 } } },
    { 3, { { 0, Ma, 8 }, { 4, Mi, 8 }, { 5, Ma, 8 } } },
    { 3, { { 5, Ma, 8 }, { 4, Mi, 8 }, { 9, Mi, 8 } } },
    { 3, { { 0, Ma, 8 }, { 9, Mi, 8 }, { 5, Ma, 8 } } },
    { 4, { { 5, Ma, 8 }, { 7, Ma, 8 }, { 9, Mi, 8 }, { 0, Ma, 8 } } },
    { 3, { { 0, Ma, 8 }, { 2, Mi, 8 }, { 5, Ma, 8 } } },
    { 3, { { 5, Ma, 8 }, { 0, Ma, 8 }, { 2, Mi, 8 } } },
    { 3, { { 9, Mi, 8 }, { 5, Ma, 8 }, { 0, Ma, 8 } } },
    { 4, { { 0, Ma, 4 }, { 5, Ma, 4 }, { 9, Mi, 4 }, { 7, Ma, 4 } } },
    { 3, { { 0, Ma, 8 }, { 7, Ma, 8 }, { 5, Ma, 8 } } },
    { 3, { { 5, Ma, 8 }, { 9, Mi, 8 }, { 0, Ma, 8 } } },
    { 3, { { 4, Mi, 8 }, { 5, Ma, 8 }, { 0, Ma, 8 } } },
    { 3, { { 0, Ma, 8 }, { 5, Ma, 8 }, { 4, Mi, 8 } } },
    { 4, { { 0, Ma, 8 }, { 9, Mi, 8 }, { 5, Ma, 8 }, { 7, Ma, 8 } } },
    { 3, { { 5, Ma, 8 }, { 2, Mi, 8 }, { 0, Ma, 8 } } },
    { 4, { { 0, X7, 8 }, { 5, X7, 8 }, { 4, M7, 8 }, { 9, M7, 8 } } },
    { 4, { { 0, S2, 8 }, { 5, A9, 8 }, { 9, M9, 8 }, { 7, S4, 8 } } },
    { 3, { { 5, X7, 8 }, { 0, A9, 8 }, { 2, M9, 8 } } },

    // With a bass line that walks (an inversion in the bass)
    { 3, { { 0, Ma, 8 }, { 7, Ma, 8, 1 }, { 9, Mi, 8 } } },
    { 3, { { 5, Ma, 8 }, { 0, Ma, 8, 1 }, { 2, Mi, 8 } } },
};

// ---- Dark: heavy and threatening, minor, few notes, close to the tonic -------------------------------------------
constexpr Entry darkMinor[] = {
    { 2, { { 0, Mi, 8 }, { 8, Ma, 8 } } },
    { 2, { { 0, Mi, 8 }, { 5, Mi, 8 } } },
    { 3, { { 0, Mi, 4 }, { 8, Ma, 4 }, { 7, Mi, 8 } } },
    { 4, { { 0, Mi, 4 }, { 0, Mi, 4 }, { 8, Ma, 4 }, { 7, Mi, 4 } } },
    { 3, { { 0, Mi, 8 }, { 10, Ma, 4 }, { 8, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 5, Mi, 4 }, { 0, Mi, 4 }, { 7, Mi, 4 } } },
    { 4, { { 0, Mi, 4 }, { 8, Ma, 4 }, { 0, Mi, 4 }, { 7, Mi, 4 } } },
    { 3, { { 0, Mi, 8 }, { 7, Mi, 4 }, { 5, Mi, 4 } } },
    { 4, { { 0, Mi, 4 }, { 10, Ma, 4 }, { 0, Mi, 4 }, { 8, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 0, Mi, 4 }, { 5, Mi, 4 }, { 8, Ma, 4 } } },
    { 3, { { 0, Mi, 8 }, { 3, Ma, 4 }, { 7, Mi, 4 } } },
    { 4, { { 0, Mi, 4 }, { 7, Mi, 4 }, { 0, Mi, 4 }, { 3, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 3, Ma, 4 }, { 0, Mi, 4 }, { 7, Mi, 4 } } },
    { 3, { { 0, Mi, 8 }, { 7, Mi, 4 }, { 10, Ma, 4 } } },
    { 2, { { 0, Mi, 8 }, { 3, Ma, 8 } } },
    { 3, { { 0, Mi, 8 }, { 1, Ma, 4 }, { 0, Mi, 4 } } },
    { 4, { { 0, Mi, 4 }, { 1, Ma, 4 }, { 0, Mi, 4 }, { 8, Ma, 4 } } },
    { 3, { { 0, Mi, 8 }, { 8, Ma, 4 }, { 1, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 8, Ma, 4 }, { 10, Mi, 4 }, { 0, Mi, 4 } } },
    { 4, { { 0, Mi, 4 }, { 0, P5, 4 }, { 8, P5, 4 }, { 7, P5, 4 } } },
};

constexpr Entry darkMajor[] = {
    { 3, { { 9, Mi, 8 }, { 4, Mi, 4 }, { 5, Ma, 4 } } },
    { 4, { { 9, Mi, 4 }, { 9, Mi, 4 }, { 5, Ma, 4 }, { 4, Mi, 4 } } },
    { 2, { { 9, Mi, 8 }, { 5, Ma, 8 } } },
    { 2, { { 9, Mi, 8 }, { 4, Mi, 8 } } },
    { 3, { { 9, Mi, 8 }, { 7, Ma, 4 }, { 5, Ma, 4 } } },
    { 4, { { 9, Mi, 4 }, { 4, Mi, 4 }, { 9, Mi, 4 }, { 5, Ma, 4 } } },
    { 4, { { 9, Mi, 4 }, { 5, Ma, 4 }, { 9, Mi, 4 }, { 4, Mi, 4 } } },
    { 3, { { 4, Mi, 8 }, { 5, Ma, 4 }, { 9, Mi, 4 } } },
    { 4, { { 9, Mi, 4 }, { 2, Mi, 4 }, { 9, Mi, 4 }, { 4, Mi, 4 } } },
    { 3, { { 9, Mi, 8 }, { 2, Mi, 4 }, { 4, Mi, 4 } } },
    { 4, { { 9, Mi, 4 }, { 7, Ma, 4 }, { 9, Mi, 4 }, { 5, Ma, 4 } } },
    { 3, { { 2, Mi, 8 }, { 9, Mi, 4 }, { 4, Mi, 4 } } },
    { 4, { { 4, Mi, 4 }, { 4, Mi, 4 }, { 9, Mi, 4 }, { 5, Ma, 4 } } },
    { 4, { { 9, Mi, 4 }, { 4, Mi, 4 }, { 2, Mi, 4 }, { 9, Mi, 4 } } },
    { 3, { { 9, Mi, 8 }, { 5, Ma, 4 }, { 7, Ma, 4 } } },
    { 2, { { 2, Mi, 8 }, { 9, Mi, 8 } } },
    { 4, { { 9, Mi, 4 }, { 0, Ma, 4 }, { 9, Mi, 4 }, { 5, Ma, 4 } } },
    { 3, { { 9, Mi, 8 }, { 0, Ma, 4 }, { 4, Mi, 4 } } },
    { 4, { { 9, Mi, 4 }, { 9, P5, 4 }, { 5, P5, 4 }, { 4, P5, 4 } } },
    { 4, { { 9, M7, 4 }, { 5, X7, 4 }, { 9, M7, 4 }, { 4, M7, 4 } } },
};

// ---- Deep: hypnotic and spacious, long chords, m7, m9, sus ------------------------------------------------------
constexpr Entry deepMinor[] = {
    { 2, { { 0, Mi, 8 }, { 5, Mi, 8 } } },
    { 2, { { 0, Mi, 8 }, { 7, Mi, 8 } } },
    { 3, { { 0, Mi, 8 }, { 5, Mi, 8 }, { 7, Mi, 8 } } },
    { 2, { { 0, Mi, 16 }, { 8, Ma, 8 } } },
    { 2, { { 0, Mi, 8 }, { 10, Ma, 8 } } },
    { 3, { { 0, Mi, 8 }, { 0, Mi, 8 }, { 5, Mi, 8 } } },
    { 3, { { 5, Mi, 8 }, { 0, Mi, 8 }, { 7, Mi, 8 } } },
    { 2, { { 5, Mi, 8 }, { 0, Mi, 8 } } },
    { 3, { { 0, Mi, 8 }, { 3, Ma, 8 }, { 5, Mi, 8 } } },
    { 3, { { 0, Mi, 8 }, { 8, Ma, 8 }, { 5, Mi, 8 } } },
    { 3, { { 0, Mi, 8 }, { 7, Mi, 8 }, { 3, Ma, 8 } } },
    { 3, { { 7, Mi, 8 }, { 0, Mi, 8 }, { 3, Ma, 8 } } },
    { 3, { { 0, Mi, 8 }, { 3, Ma, 8 }, { 7, Mi, 8 } } },
    { 3, { { 0, Mi, 8 }, { 5, Ma, 8 }, { 10, Ma, 8 } } },
    { 2, { { 0, Mi, 8 }, { 5, Ma, 8 } } },
    { 3, { { 0, Mi, 8 }, { 2, Mi, 8 }, { 5, Ma, 8 } } },
    { 2, { { 0, Mi, 8 }, { 1, Ma, 8 } } },
    { 3, { { 0, Mi, 8 }, { 1, Ma, 8 }, { 8, Ma, 8 } } },
    { 3, { { 0, M7, 8 }, { 5, M7, 8 }, { 7, M7, 8 } } },
    { 3, { { 0, M9, 8 }, { 5, M9, 8 }, { 0, S4, 8 } } },
};

constexpr Entry deepMajor[] = {
    { 2, { { 9, Mi, 8 }, { 4, Mi, 8 } } },
    { 2, { { 9, Mi, 8 }, { 2, Mi, 8 } } },
    { 3, { { 9, Mi, 8 }, { 2, Mi, 8 }, { 4, Mi, 8 } } },
    { 2, { { 2, Mi, 8 }, { 9, Mi, 8 } } },
    { 2, { { 4, Mi, 8 }, { 9, Mi, 8 } } },
    { 2, { { 0, Ma, 8 }, { 9, Mi, 8 } } },
    { 3, { { 9, Mi, 8 }, { 5, Ma, 8 }, { 0, Ma, 8 } } },
    { 2, { { 5, Ma, 8 }, { 9, Mi, 8 } } },
    { 3, { { 2, Mi, 8 }, { 4, Mi, 8 }, { 9, Mi, 8 } } },
    { 3, { { 9, Mi, 8 }, { 4, Mi, 8 }, { 5, Ma, 8 } } },
    { 2, { { 0, Ma, 8 }, { 4, Mi, 8 } } },
    { 3, { { 0, Ma, 8 }, { 2, Mi, 8 }, { 9, Mi, 8 } } },
    { 2, { { 2, Mi, 16 }, { 9, Mi, 8 } } },
    { 3, { { 4, Mi, 8 }, { 2, Mi, 8 }, { 9, Mi, 8 } } },
    { 3, { { 9, Mi, 8 }, { 7, Ma, 8 }, { 5, Ma, 8 } } },
    { 2, { { 5, Ma, 8 }, { 4, Mi, 8 } } },
    { 3, { { 9, Mi, 8 }, { 0, Ma, 8 }, { 5, Ma, 8 } } },
    { 2, { { 0, Ma, 8 }, { 2, Mi, 8 } } },
    { 3, { { 2, M7, 8 }, { 9, M7, 8 }, { 4, M7, 8 } } },
    { 3, { { 9, M9, 8 }, { 2, M9, 8 }, { 0, S4, 8 } } },
};

// ---- Driving: forward and energetic, triads and power chords ------------------------------------------------------
constexpr Entry drivingMinor[] = {
    { 4, { { 0, Mi, 4 }, { 8, Ma, 4 }, { 10, Ma, 4 }, { 0, Mi, 4 } } },
    { 4, { { 0, Mi, 4 }, { 10, Ma, 4 }, { 8, Ma, 4 }, { 10, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 3, Ma, 4 }, { 10, Ma, 4 }, { 8, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 0, Mi, 4 }, { 8, Ma, 4 }, { 10, Ma, 4 } } },
    { 4, { { 8, Ma, 4 }, { 10, Ma, 4 }, { 0, Mi, 4 }, { 0, Mi, 4 } } },
    { 4, { { 0, Mi, 4 }, { 5, Mi, 4 }, { 8, Ma, 4 }, { 10, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 10, Ma, 4 }, { 3, Ma, 4 }, { 8, Ma, 4 } } },
    { 4, { { 8, Ma, 4 }, { 10, Ma, 4 }, { 3, Ma, 4 }, { 0, Mi, 4 } } },
    { 4, { { 0, Mi, 2 }, { 0, Mi, 2 }, { 8, Ma, 4 }, { 10, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 8, Ma, 4 }, { 3, Ma, 4 }, { 10, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 7, Mi, 4 }, { 8, Ma, 4 }, { 10, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 7, Mi, 4 }, { 10, Ma, 4 }, { 8, Ma, 4 } } },
    { 4, { { 10, Ma, 4 }, { 8, Ma, 4 }, { 0, Mi, 4 }, { 0, Mi, 4 } } },
    { 4, { { 0, Mi, 4 }, { 3, Ma, 4 }, { 7, Mi, 4 }, { 10, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 10, Ma, 4 }, { 3, Ma, 4 }, { 7, Mi, 4 } } },
    { 4, { { 0, Mi, 4 }, { 5, Ma, 4 }, { 10, Ma, 4 }, { 3, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 10, Ma, 4 }, { 5, Ma, 4 }, { 0, Mi, 4 } } },
    { 4, { { 0, Mi, 4 }, { 1, Ma, 4 }, { 8, Ma, 4 }, { 1, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 8, Ma, 4 }, { 1, Ma, 4 }, { 0, Mi, 4 } } },
    { 4, { { 0, P5, 4 }, { 8, P5, 4 }, { 10, P5, 4 }, { 0, P5, 4 } } },
    { 4, { { 0, Mi, 4 }, { 8, Ma, 4 }, { 3, Ma, 4 }, { 8, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 3, Ma, 4 }, { 8, Ma, 4 }, { 5, Mi, 4 } } },
    { 4, { { 0, Mi, 4 }, { 5, Mi, 4 }, { 3, Ma, 4 }, { 8, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 5, Ma, 4 }, { 3, Ma, 4 }, { 5, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 2, Mi, 4 }, { 5, Ma, 4 }, { 3, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 5, Ma, 4 }, { 10, Ma, 4 }, { 0, Mi, 4 } } },
};

constexpr Entry drivingMajor[] = {
    { 4, { { 0, Ma, 4 }, { 5, Ma, 4 }, { 7, Ma, 4 }, { 0, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 7, Ma, 4 }, { 5, Ma, 4 }, { 0, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 7, Ma, 4 }, { 9, Mi, 4 }, { 5, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 5, Ma, 4 }, { 0, Ma, 4 }, { 7, Ma, 4 } } },
    { 4, { { 5, Ma, 4 }, { 0, Ma, 4 }, { 7, Ma, 4 }, { 7, Ma, 4 } } },
    { 4, { { 0, Ma, 2 }, { 0, Ma, 2 }, { 5, Ma, 4 }, { 7, Ma, 4 } } },
    { 4, { { 9, Mi, 4 }, { 5, Ma, 4 }, { 0, Ma, 4 }, { 7, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 5, Ma, 4 }, { 7, Ma, 4 }, { 5, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 9, Mi, 4 }, { 5, Ma, 4 }, { 7, Ma, 4 } } },
    { 4, { { 5, Ma, 4 }, { 7, Ma, 4 }, { 0, Ma, 4 }, { 9, Mi, 4 } } },
    { 4, { { 0, Ma, 4 }, { 7, Ma, 4 }, { 0, Ma, 4 }, { 5, Ma, 4 } } },
    { 4, { { 7, Ma, 4 }, { 5, Ma, 4 }, { 0, Ma, 4 }, { 7, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 4, Mi, 4 }, { 5, Ma, 4 }, { 7, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 2, Mi, 4 }, { 5, Ma, 4 }, { 7, Ma, 4 } } },
    { 4, { { 9, Mi, 4 }, { 7, Ma, 4 }, { 5, Ma, 4 }, { 7, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 5, Ma, 4 }, { 9, Mi, 4 }, { 5, Ma, 4 } } },
    { 4, { { 5, Ma, 4 }, { 5, Ma, 4 }, { 0, Ma, 4 }, { 7, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 7, Ma, 4 }, { 2, Mi, 4 }, { 7, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 5, Ma, 4 }, { 2, Mi, 4 }, { 7, Ma, 4 } } },
    { 4, { { 0, P5, 4 }, { 5, P5, 4 }, { 7, P5, 4 }, { 0, P5, 4 } } },
};

// ---- Soulful: rich and warm, 7, m7, 9 ----------------------------------------------------------------------------
constexpr Entry soulfulMinor[] = {
    { 4, { { 0, Mi, 4 }, { 5, Mi, 4 }, { 10, Ma, 4 }, { 3, Ma, 4 } } },
    { 4, { { 8, Ma, 4 }, { 10, Ma, 4 }, { 0, Mi, 4 }, { 0, Mi, 4 } } },
    { 4, { { 0, Mi, 4 }, { 3, Ma, 4 }, { 5, Mi, 4 }, { 10, Ma, 4 } } },
    { 4, { { 5, Mi, 4 }, { 10, Ma, 4 }, { 3, Ma, 4 }, { 0, Mi, 4 } } },
    { 4, { { 0, Mi, 4 }, { 8, Ma, 4 }, { 5, Mi, 4 }, { 10, Ma, 4 } } },
    { 4, { { 3, Ma, 4 }, { 5, Mi, 4 }, { 0, Mi, 4 }, { 10, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 10, Ma, 4 }, { 3, Ma, 4 }, { 5, Mi, 4 } } },
    { 4, { { 5, Mi, 4 }, { 0, Mi, 4 }, { 8, Ma, 4 }, { 10, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 5, Mi, 4 }, { 0, Mi, 4 }, { 10, Ma, 4 } } },
    { 3, { { 0, Mi, 4 }, { 5, Mi, 4 }, { 3, Ma, 8 } } },
    { 4, { { 8, Ma, 4 }, { 5, Mi, 4 }, { 10, Ma, 4 }, { 0, Mi, 4 } } },
    { 4, { { 0, Mi, 4 }, { 7, Mi, 4 }, { 5, Mi, 4 }, { 10, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 3, Ma, 4 }, { 7, Mi, 4 }, { 5, Mi, 4 } } },
    { 4, { { 0, Mi, 4 }, { 3, Ma, 4 }, { 10, Ma, 4 }, { 7, Mi, 4 } } },
    { 4, { { 5, Ma, 4 }, { 0, Mi, 4 }, { 3, Ma, 4 }, { 10, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 5, Ma, 4 }, { 10, Ma, 4 }, { 5, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 5, Mi, 4 }, { 1, Ma, 4 }, { 0, Mi, 4 } } },
    { 4, { { 0, M7, 4 }, { 5, M7, 4 }, { 10, D7, 4 }, { 3, X7, 4 } } },
    { 4, { { 0, M9, 4 }, { 5, M7, 4 }, { 10, N9, 4 }, { 3, X7, 4 } } },
    { 4, { { 8, X7, 4 }, { 10, D7, 4 }, { 0, M7, 4 }, { 5, M9, 4 } } },
    { 4, { { 0, Mi, 4 }, { 8, Ma, 4 }, { 3, Ma, 4 }, { 5, Mi, 4 } } },
    { 4, { { 3, Ma, 4 }, { 0, Mi, 4 }, { 8, Ma, 4 }, { 5, Mi, 4 } } },
    { 4, { { 0, Mi, 4 }, { 5, Mi, 4 }, { 3, Ma, 4 }, { 8, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 2, Mi, 4 }, { 5, Ma, 4 }, { 10, Ma, 4 } } },
    { 4, { { 0, Mi, 4 }, { 5, Ma, 4 }, { 2, Mi, 4 }, { 3, Ma, 4 } } },
    { 4, { { 5, Ma, 4 }, { 3, Ma, 4 }, { 0, Mi, 4 }, { 7, Mi, 4 } } },
    { 4, { { 0, Mi, 4 }, { 1, Ma, 4 }, { 3, Ma, 4 }, { 0, Mi, 4 } } },
    { 4, { { 8, Ma, 4 }, { 1, Ma, 4 }, { 3, Ma, 4 }, { 0, Mi, 4 } } },

    // With a bass line that walks (an inversion in the bass)
    { 4, { { 8, Ma, 4 }, { 3, Ma, 4, 1 }, { 5, Mi, 4 }, { 0, Mi, 4 } } },
    { 4, { { 0, Mi, 4 }, { 7, Mi, 4, 1 }, { 8, Ma, 4 }, { 5, Mi, 4 } } },
};

constexpr Entry soulfulMajor[] = {
    { 4, { { 0, Ma, 4 }, { 9, Mi, 4 }, { 2, Mi, 4 }, { 7, Ma, 4 } } },
    { 4, { { 2, Mi, 4 }, { 7, Ma, 4 }, { 0, Ma, 4 }, { 9, Mi, 4 } } },
    { 4, { { 0, Ma, 4 }, { 4, Mi, 4 }, { 9, Mi, 4 }, { 5, Ma, 4 } } },
    { 4, { { 5, Ma, 4 }, { 4, Mi, 4 }, { 2, Mi, 4 }, { 7, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 5, Ma, 4 }, { 2, Mi, 4 }, { 7, Ma, 4 } } },
    { 4, { { 9, Mi, 4 }, { 2, Mi, 4 }, { 7, Ma, 4 }, { 0, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 9, Mi, 4 }, { 5, Ma, 4 }, { 7, Ma, 4 } } },
    { 4, { { 4, Mi, 4 }, { 9, Mi, 4 }, { 2, Mi, 4 }, { 7, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 2, Mi, 4 }, { 4, Mi, 4 }, { 5, Ma, 4 } } },
    { 4, { { 5, Ma, 4 }, { 0, Ma, 4 }, { 2, Mi, 4 }, { 7, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 5, Ma, 4 }, { 4, Mi, 4 }, { 9, Mi, 4 } } },
    { 4, { { 2, Mi, 4 }, { 4, Mi, 4 }, { 5, Ma, 4 }, { 0, Ma, 4 } } },
    { 4, { { 0, Ma, 4 }, { 9, Mi, 4 }, { 4, Mi, 4 }, { 5, Ma, 4 } } },
    { 4, { { 9, Mi, 4 }, { 5, Ma, 4 }, { 2, Mi, 4 }, { 7, Ma, 4 } } },
    { 3, { { 0, Ma, 4 }, { 2, Mi, 4 }, { 7, Ma, 8 } } },
    { 4, { { 5, Ma, 4 }, { 7, Ma, 4 }, { 4, Mi, 4 }, { 9, Mi, 4 } } },
    { 4, { { 0, Ma, 4 }, { 4, Mi, 4 }, { 2, Mi, 4 }, { 7, Ma, 4 } } },
    { 4, { { 0, X7, 4 }, { 9, M7, 4 }, { 2, M7, 4 }, { 7, D7, 4 } } },
    { 4, { { 2, M7, 4 }, { 7, D7, 4 }, { 0, X7, 4 }, { 9, M7, 4 } } },
    { 4, { { 0, A9, 4 }, { 4, M7, 4 }, { 5, X7, 4 }, { 7, N9, 4 } } },

    // With a bass line that walks (an inversion in the bass)
    { 4, { { 2, Mi, 4 }, { 0, Ma, 4, 1 }, { 5, Ma, 4 }, { 7, Ma, 4 } } },
    { 4, { { 5, Ma, 4 }, { 4, Mi, 4 }, { 2, Mi, 4 }, { 0, Ma, 4, 1 } } },
};

List listFor (Mood mood, bool major)
{
    switch (mood)
    {
        case Mood::Bright:      return major ? listOf (brightMajor)      : listOf (brightMinor);
        case Mood::Warm:        return major ? listOf (warmMajor)        : listOf (warmMinor);
        case Mood::Dreamy:      return major ? listOf (dreamyMajor)      : listOf (dreamyMinor);
        case Mood::Melancholic: return major ? listOf (melancholicMajor) : listOf (melancholicMinor);
        case Mood::Dark:        return major ? listOf (darkMajor)        : listOf (darkMinor);
        case Mood::Deep:        return major ? listOf (deepMajor)        : listOf (deepMinor);
        case Mood::Driving:     return major ? listOf (drivingMajor)     : listOf (drivingMinor);
        case Mood::Soulful:     return major ? listOf (soulfulMajor)     : listOf (soulfulMinor);
    }
    return { nullptr, 0 };
}

Progression toProgression (const Entry& entry, bool withInversions = true)
{
    Progression result;
    for (int i = 0; i < entry.count; ++i)
        result.push_back ({ { entry.steps[i].root, entry.steps[i].type, withInversions ? entry.steps[i].inversion : kAutoInversion }, entry.steps[i].beats, false });
    return result;
}

bool hasInversion (const Entry& entry)
{
    for (int i = 0; i < entry.count; ++i)
        if (entry.steps[i].inversion != kAutoInversion)
            return true;
    return false;
}

bool entryFits (const Entry& entry, ScaleId scale)
{
    for (int i = 0; i < entry.count; ++i)
        if (! chordFitsScale ({ entry.steps[i].root, entry.steps[i].type, kAutoInversion }, scale))
            return false;
    return true;
}

bool entryOnPads (const Entry& entry, ScaleId scale)
{
    for (int i = 0; i < entry.count; ++i)
        if (! isPadChord ({ entry.steps[i].root, entry.steps[i].type, kAutoInversion }, scale))
            return false;
    return true;
}

// Four chords from the pads of the scale, when nothing in the library fits (it should not happen for the scales we have).
// These are always in the scale.
Progression fromPads (ScaleId scale, unsigned randomValue)
{
    static constexpr int patterns[][4] = { { 0, 5, 2, 6 }, { 0, 3, 5, 6 }, { 0, 6, 5, 6 }, { 0, 2, 5, 3 } };
    const auto& pattern = patterns[randomValue % 4];
    Progression result;
    for (int degree : pattern)
        result.push_back ({ padChord (scale, degree), kDefaultBlockBeats, false });
    return result;
}
}

int libraryCount (Mood mood, bool major)
{
    return listFor (mood, major).count;
}

Progression libraryProgression (Mood mood, bool major, int index, const Key&)
{
    const auto list = listFor (mood, major);
    if (index < 0 || index >= list.count)
        return {};
    return toProgression (list.entries[index]); // chords are distances from the tonic, so the key changes nothing here
}

bool libraryEntryFits (Mood mood, bool major, int index, ScaleId scale)
{
    const auto list = listFor (mood, major);
    return index >= 0 && index < list.count && entryFits (list.entries[index], scale);
}

bool libraryEntryOnPads (Mood mood, bool major, int index, ScaleId scale)
{
    const auto list = listFor (mood, major);
    return index >= 0 && index < list.count && entryFits (list.entries[index], scale) && entryOnPads (list.entries[index], scale);
}

bool libraryEntryHasInversion (Mood mood, bool major, int index)
{
    const auto list = listFor (mood, major);
    return index >= 0 && index < list.count && hasInversion (list.entries[index]);
}

Suggestion suggest (const Key& key, Mood mood, unsigned randomValue, int avoid, bool allowInversions)
{
    const bool major = key.scale == ScaleId::Major;
    const auto list = listFor (mood, major);

    int candidates[64];
    int numCandidates = 0;
    for (int i = 0; i < list.count && numCandidates < 64; ++i)
        if (entryFits (list.entries[i], key.scale) && entryOnPads (list.entries[i], key.scale) && (allowInversions || ! hasInversion (list.entries[i])))
            candidates[numCandidates++] = i;

    if (numCandidates == 0)
        return { fromPads (key.scale, randomValue), -1 };

    int pick = (int) (randomValue % (unsigned) numCandidates);
    if (numCandidates > 1 && candidates[pick] == avoid)
        pick = (pick + 1) % numCandidates;

    return { toProgression (list.entries[candidates[pick]], allowInversions), candidates[pick] };
}
}
