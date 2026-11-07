// Copyright (C) 2026 Henrik Juhl. Part of Progrezzor, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "Progression.h"

// The library of progressions, written by hand for this project (no copied lists, no names of songs or artists), and
// Suggest, which picks from it. Pure C++, no JUCE.
//
// The library is data, independent of the key: every chord is a distance from the tonic. There is one list for minor
// and one for major per mood. Dorian and Phrygian use the minor ones, and only those whose chords all fit the scale.
namespace engine
{
enum class Mood { Bright, Warm, Dreamy, Melancholic, Dark, Deep, Driving, Soulful };
constexpr int kNumMoods = 8;

constexpr const char* moodNames[kNumMoods] = { "Bright", "Warm", "Dreamy", "Melancholic", "Dark", "Deep", "Driving", "Soulful" };

// How many progressions the library has for a mood, and in a major or minor key (so far only Melancholic has any)
int libraryCount (Mood mood, bool major);

// The progression with the given number, in the key. Lock to scale does not matter here: the library fits the scale
// of its kind (see fitsScale), and Suggest only picks those that fit.
Progression libraryProgression (Mood mood, bool major, int index, const Key& key);

// A progression is "major" if the scale is Major, otherwise minor. True if all its chords fit the scale of the key.
bool libraryEntryFits (Mood mood, bool major, int index, ScaleId scale);

// True if every chord of the progression is one of the seven pads of the scale. Suggest only picks such progressions, so
// that everything in the bar can also be played on the pads (chosen by Henrik after the test of milestone 3).
bool libraryEntryOnPads (Mood mood, bool major, int index, ScaleId scale);

// One suggestion: a whole progression for the key and mood. `randomValue` is any number (the caller's randomness), so that
// the function itself is predictable and can be tested. `avoid` is the progression that was suggested last (as a number
// from a previous result's `index`, or -1) and is not suggested again if there is any other. The result always fits the
// scale of the key, and every chord in it is a pad.
struct Suggestion
{
    Progression blocks;
    int index = -1; // the library number it came from, -1 if it was made from the pads (no library entry fits)
};

Suggestion suggest (const Key& key, Mood mood, unsigned randomValue, int avoid);
}
