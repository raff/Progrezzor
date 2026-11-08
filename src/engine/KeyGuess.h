// Copyright (C) 2026 Henrik Juhl. Part of Progrezzor, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <vector>

#include "ChordEngine.h"

// Guessing the key (major or minor) from a list of chords. Pure C++, no JUCE. It only gives a guess, and the caller asks the
// user before anything is changed.
//
// A key fits if every chord is in its scale. In a minor key the major chord on the fifth step (E in A Minor) fits too.
// Every major key has a minor twin with the same chords (G Major and E Minor); between the two, the minor wins if the first or
// last chord is the minor chord of the twin, the major wins if the first or last chord is the major one, else the one whose key
// chord is used most, and a tie is major. If several different keys fit, the one whose key chord is the first or last chord wins,
// else the one whose key chord is used most; a tie gives no guess. If no key fits there is no guess.
namespace engine
{
// A chord named by its root as a pitch class (0 = C ... 11 = B), not as a distance from a tonic
struct GuessChord
{
    int root = 0;
    ChordType type = ChordType::Major;
};

struct KeyGuess
{
    bool found = false;
    Key key;
};

KeyGuess guessKey(const std::vector<GuessChord>& chords);

// True if two keys are the same for this purpose: the same tonic, and both major or both minor (Minor, Dorian and Phrygian count
// as minor)
bool sameKeyKind(const Key& a, const Key& b);
}
