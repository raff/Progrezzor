// Copyright (C) 2026 Henrik Juhl. Part of Progrezzor, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <string>
#include <vector>

#include "Progression.h"

// Reading a ChordPro file for its chords. Pure C++, no JUCE. Only the chords in square brackets are used, in the order they are
// written: the words of the song, comments (# lines) and every directive except the sections and the key are ignored. The chords
// are handed on as plain words, so that Quick-text can read them (and mark what it does not know). Nothing is corrected.
//
// Sections are {start_of_verse} {start_of_chorus} {start_of_bridge} (short: {sov} {soc} {sob}), with an optional label after a
// colon, and their {end_of_...} directives. Chords outside a section are not part of any section.
//
// Grids ({start_of_grid} ... {end_of_grid}, short {sog} {eog}) are read too: the cells between the bar lines are beats, a chord
// starts a block, "." or "/" lets it go on, "N.C." is a pause, % repeats the measure before and %% the two before. The length of a
// block is how many cells it takes of a measure (shape="4x4" says how many cells a measure has, else the cells of the measure
// itself are counted). Bar line symbols (|| |: :| |1 ...) only separate measures; repeats are not played out. Margins and
// strum lines are skipped. Each block comes out as a Quick-text word with its length in bars (C, C:2, G:0.5).
namespace engine
{
struct ChordProSection
{
    std::string name;                 // "Verse", "Chorus 2", or the label of the file
    std::vector<std::string> chords;  // the chords of the section, in order
};

struct ChordProResult
{
    std::vector<std::string> chords;       // every chord of the file, in order
    std::vector<ChordProSection> sections; // only the sections that have chords in them
    bool hasKey = false;                   // the file says {key: ...} and it is one we can read
    std::string keyName;                   // as written: G, F#m, Bb
    int keyTonic = 0;                      // 0 = C ... 11 = B
    bool keyMinor = false;
};

ChordProResult readChordPro(const std::string& text);

// True for the file name endings of ChordPro files: .cho .chopro .crd .pro (also in capitals)
bool isChordProFileName(const std::string& fileName);

// The chords as Quick-text, one bar each: "C Am F G". At most `maxChords`; `tooMany` says whether some were left out.
std::string chordsAsQuickText(const std::vector<std::string>& chords, int maxChords, bool* tooMany = nullptr);

// A ChordPro file for the progression: the title, the key and the chords as a grid with the cells of a measure as beats, so that
// the lengths are kept (a chord that goes on is written as ".", a pause as "N.C."). Octaves are not written.
// `barBeats` is the length of a bar in beats.
std::string exportChordPro(const Progression&, const Key&, double barBeats, const std::string& title = "Progrezzor");

// Does the song's key differ from the key of the plug-in? (Same tonic, and both major or both minor, is the same.)
bool chordProKeyDiffers(const ChordProResult&, const Key&);
}
