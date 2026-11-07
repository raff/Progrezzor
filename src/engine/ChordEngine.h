// Copyright (C) 2026 Henrik Juhl. Part of Progrezzor, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <array>
#include <string>

#include "Music.h"

// The chord engine. Pure C++, no JUCE, no allocation in voiceChord(), so it can be called from the audio thread.
//
// A chord is stored the way the CLAUDE.md says: distance from the tonic in semitones, chord type, inversion. Never as a
// pad number or as finished MIDI notes. So a progression follows the key when the key changes, and changing a pad's
// variant never changes a progression.
namespace engine
{
enum class ChordType
{
    Major, Minor, Power5, Dom7, Min7, Maj7, Dom9, Min9, Add9, Sus2, Sus4, Dim
};
constexpr int kNumChordTypes = 12;

constexpr int kMaxChordNotes = 5;

struct ChordShape
{
    const char* suffix; // as written after the root name: "", "m", "7" ...
    int count;
    int intervals[kMaxChordNotes]; // semitones above the root, ascending (a ninth is 14)
};

constexpr ChordShape chordShapes[kNumChordTypes] = {
    { "", 3, { 0, 4, 7 } },
    { "m", 3, { 0, 3, 7 } },
    { "5", 2, { 0, 7 } },
    { "7", 4, { 0, 4, 7, 10 } },
    { "m7", 4, { 0, 3, 7, 10 } },
    { "maj7", 4, { 0, 4, 7, 11 } },
    { "9", 5, { 0, 4, 7, 10, 14 } },
    { "m9", 5, { 0, 3, 7, 10, 14 } },
    { "add9", 4, { 0, 4, 7, 14 } },
    { "sus2", 3, { 0, 2, 7 } },
    { "sus4", 3, { 0, 5, 7 } },
    { "dim", 3, { 0, 3, 6 } },
};

constexpr const ChordShape& shapeOf (ChordType type) { return chordShapes[(int) type]; }

// Inversion: 0 is root position, 1 the first inversion and so on. kAutoInversion lets the voice leading choose, a number
// is the user's own choice and is always respected. A number past the last note wraps round.
constexpr int kAutoInversion = -1;

// A chord can be moved whole octaves up or down (the Octave up and down buttons). It is a part of the chord, so it follows the
// chord from a pad into the bar, but it is not in the name.
constexpr int kMaxOctaveShift = 2;

struct Chord
{
    int root = 0; // semitones above the tonic, 0 to 11
    ChordType type = ChordType::Major;
    int inversion = kAutoInversion;
    int octave = 0; // whole octaves up (+) or down (-), -kMaxOctaveShift to kMaxOctaveShift

    friend constexpr bool operator== (const Chord& a, const Chord& b)
    {
        return a.root == b.root && a.type == b.type && a.inversion == b.inversion && a.octave == b.octave;
    }
    friend constexpr bool operator!= (const Chord& a, const Chord& b) { return ! (a == b); }
};

// The notes of a chord, as MIDI note numbers, ascending.
struct Voicing
{
    std::array<int, kMaxChordNotes> notes {};
    int count = 0;
    int octave = 0; // the octaves the notes have been moved (already in `notes`), so that the next chord can follow the unmoved voicing
};

// ---- The seven pads ---------------------------------------------------------------------------------------------

// Pad 0 to 6 are built by stacking scale notes (root, third and fifth from the scale), so they always fit the scale.
Chord padChord (ScaleId scale, int degree);

// True if the chord is one of the seven pads of the scale (the inversion does not matter).
bool isPadChord (const Chord& chord, ScaleId scale);

// ---- Scale checks (for Lock to scale) ----------------------------------------------------------------------------

bool noteInScale (ScaleId scale, int semitonesFromTonic);

// True if every note of the chord is in the scale. Sus4 therefore only fits where the pure fourth is in the scale, and
// sus2 where the major second is.
bool chordFitsScale (const Chord& chord, ScaleId scale);

// ---- Names -----------------------------------------------------------------------------------------------------

// Whether the names of this key are written with flats (Bb) rather than sharps (A#). It follows the key signature.
bool keyUsesFlats (const Key& key);

std::string noteName (int pitchClass, bool flats);

// The name of the key itself: "A", "Bb"
std::string keyName (const Key& key);

// "Am", "Bbmaj7". A chosen inversion adds the bass note: "Am/C".
std::string chordName (const Chord& chord, const Key& key);

// ---- Voicing -----------------------------------------------------------------------------------------------------

constexpr int kLowestNote = 36;  // C2
constexpr int kHighestNote = 96; // C7
constexpr int kCentreNote = 60;  // the chords gather around C4

// Turns a chord into MIDI notes. With `previous` (the voicing that sounded before) the chord is placed and inverted so
// that the notes move as little as possible (an octave shift of the chord or of `previous` is left out of that, so a chord
// moved up does not pull the next one up with it). Without it the chord is in root position near the centre, unless the
// chord has a chosen inversion.
Voicing voiceChord (const Chord& chord, int tonic, const Voicing* previous);
}
