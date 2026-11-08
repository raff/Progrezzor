// Copyright (C) 2026 Henrik Juhl. Part of Progrezzor, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only
#include "ChordEngine.h"

#include <algorithm>
#include <cmath>

namespace engine
{
Chord padChord (ScaleId scaleId, int degree)
{
    const auto& scale = scaleOf (scaleId);
    degree = ((degree % kScaleSize) + kScaleSize) % kScaleSize;

    // Stack two thirds on the scale note: the notes two and four steps up the scale, wrapped round the octave.
    const int root = scale.intervals[degree];
    const int third = wrap12 (scale.intervals[(degree + 2) % kScaleSize] - root);
    const int fifth = wrap12 (scale.intervals[(degree + 4) % kScaleSize] - root);

    ChordType type = ChordType::Major;
    if (third == 3 && fifth == 7)
        type = ChordType::Minor;
    else if (third == 3 && fifth == 6)
        type = ChordType::Dim;

    return { root, type, kAutoInversion };
}

bool isPadChord (const Chord& chord, ScaleId scale)
{
    for (int degree = 0; degree < kScaleSize; ++degree)
    {
        const auto pad = padChord (scale, degree);
        if (pad.root == chord.root && pad.type == chord.type)
            return true;
    }
    return false;
}

bool noteInScale (ScaleId scaleId, int semitonesFromTonic)
{
    const int pitchClass = wrap12 (semitonesFromTonic);
    for (int interval : scaleOf (scaleId).intervals)
        if (interval == pitchClass)
            return true;
    return false;
}

bool chordFitsScale (const Chord& chord, ScaleId scale)
{
    const auto& shape = shapeOf (chord.type);
    for (int i = 0; i < shape.count; ++i)
        if (! noteInScale (scale, chord.root + shape.intervals[i]))
            return false;
    return true;
}

bool keyUsesFlats (const Key& key)
{
    // The major key with the same notes decides: F, Bb, Eb, Ab, Db are flat keys; G, D, A, E, B are sharp keys.
    const int parent = wrap12 (key.tonic + scaleOf (key.scale).relativeMajorOffset);
    switch (parent)
    {
        case 5: case 10: case 3: case 8: case 1:
            return true;
        case 6: // six sharps or six flats: F# stays F#, anything else (Eb Minor, Ab Dorian) uses flats
            return key.tonic != 6;
        default:
            return false;
    }
}

std::string noteName (int pitchClass, bool flats)
{
    static const char* const sharpNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const char* const flatNames[12] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
    return (flats ? flatNames : sharpNames)[wrap12 (pitchClass)];
}

std::string keyName (const Key& key)
{
    return noteName (key.tonic, keyUsesFlats (key));
}

std::string chordName (const Chord& chord, const Key& key)
{
    const bool flats = keyUsesFlats (key);
    const auto& shape = shapeOf (chord.type);
    std::string name = noteName (key.tonic + chord.root, flats) + shape.suffix;

    if (chord.inversion > 0)
    {
        const int bassIndex = chord.inversion % shape.count;
        if (bassIndex != 0)
            name += "/" + noteName (key.tonic + chord.root + shape.intervals[bassIndex], flats);
    }
    return name;
}

std::string romanNumeral (const Chord& chord, ScaleId scale)
{
    const auto& sc = scaleOf (scale);
    const int root = wrap12 (chord.root);

    int degree = -1;
    for (int i = 0; i < kScaleSize; ++i)
        if (sc.intervals[i] == root)
            degree = i;

    std::string accidental;
    if (degree < 0)
    {
        // Not in the scale: a flat on the degree just above, or a sharp on the degree just below
        int above = -1, below = 0;
        for (int i = 0; i < kScaleSize; ++i)
        {
            if (sc.intervals[i] == wrap12 (root + 1))
                above = i;
            if (sc.intervals[i] < root)
                below = i;
        }
        const bool flat = above > 0; // not a flat of the tonic above: the note below the tonic is a sharp seventh
        accidental = flat ? "b" : "#";
        degree = flat ? above : below;
    }

    static const char* numerals[kScaleSize] = { "I", "II", "III", "IV", "V", "VI", "VII" };
    std::string numeral = numerals[degree];

    const bool small = chord.type == ChordType::Minor || chord.type == ChordType::Min7 || chord.type == ChordType::Min9 || chord.type == ChordType::Dim;
    if (small)
        for (auto& c : numeral)
            c = (char) (c - 'A' + 'a');

    const char* suffix = "";
    switch (chord.type)
    {
        case ChordType::Power5: suffix = "5"; break;
        case ChordType::Dom7:   suffix = "7"; break;
        case ChordType::Min7:   suffix = "7"; break;
        case ChordType::Maj7:   suffix = "maj7"; break;
        case ChordType::Dom9:   suffix = "9"; break;
        case ChordType::Min9:   suffix = "9"; break;
        case ChordType::Add9:   suffix = "add9"; break;
        case ChordType::Sus2:   suffix = "sus2"; break;
        case ChordType::Sus4:   suffix = "sus4"; break;
        case ChordType::Dim:    suffix = "\xC2\xB0"; break; // the degree sign
        default: break;
    }
    return accidental + numeral + suffix;
}

namespace
{
// Notes of the chord in the given inversion, ascending, with the chord's root at `rootNote` before any inversion.
Voicing invert (const Chord& chord, int rootNote, int inversion)
{
    const auto& shape = shapeOf (chord.type);
    Voicing v;
    v.count = shape.count;
    for (int i = 0; i < shape.count; ++i)
        v.notes[(size_t) i] = rootNote + shape.intervals[i] + (i < inversion ? 12 : 0);
    // The raised notes may now pass others (the root raised an octave is above a seventh, but below a ninth), so sort.
    // A plain insertion sort: five notes at most, and no warnings from the compiler about the array bounds.
    for (int i = 1; i < v.count; ++i)
        for (int j = i; j > 0 && v.notes[(size_t) j - 1] > v.notes[(size_t) j]; --j)
            std::swap (v.notes[(size_t) j - 1], v.notes[(size_t) j]);
    return v;
}
}

Voicing shiftedByOctaves (Voicing voicing, int octaves)
{
    octaves = std::clamp (octaves, -kMaxTotalOctave, kMaxTotalOctave);
    // Fewer octaves if the top or the bottom would leave the MIDI range
    while (octaves > 0 && voicing.count > 0 && voicing.notes[(size_t) voicing.count - 1] + 12 * octaves > 127)
        --octaves;
    while (octaves < 0 && voicing.count > 0 && voicing.notes[0] + 12 * octaves < 0)
        ++octaves;
    for (int i = 0; i < voicing.count; ++i)
        voicing.notes[(size_t) i] += 12 * octaves;
    return voicing;
}

namespace
{
// Total distance from each note of `v` to the nearest note of `home`.
int distanceToHome (const Voicing& v, const Voicing& home)
{
    int total = 0;
    for (int i = 0; i < v.count; ++i)
    {
        int nearest = 1000;
        for (int j = 0; j < home.count; ++j)
            nearest = std::min (nearest, std::abs (v.notes[(size_t) i] - home.notes[(size_t) j]));
        total += nearest;
    }
    return total;
}
}

Voicing voiceChord (const Chord& chord, int tonic, int centreNote, VoicingMode mode, ScaleId scale)
{
    // No magic: the notes of a chord depend only on the chord, the key and the register. Nothing that was played before has
    // any say. The tonic's root sits in the 12 semitones from centreNote - 6 (F#3 to F4 at the normal centre). Every other
    // root sits 0 to 11 semitones above it, by the chord's distance from the tonic, so the pads always climb. Root position
    // (the root is the lowest note), or the inversion that was chosen.
    const int count = shapeOf (chord.type).count;
    const int inversion = chord.inversion < 0 ? 0 : chord.inversion % count;

    const int low = centreNote - 6;
    const int tonicRoot = low + wrap12 (tonic - low);
    Voicing v = invert (chord, tonicRoot + wrap12 (chord.root), inversion);

    if (mode == VoicingMode::CloseToHome && chord.inversion < 0)
    {
        // Try every inversion in a few octaves and keep the one nearest the tonic chord. The first of equals wins (lowest
        // inversion, then lowest octave), so the answer never depends on anything but the chord.
        const Voicing home = invert (padChord (scale, 0), tonicRoot, 0);
        int best = distanceToHome (v, home) + 1;
        for (int inv = 0; inv < count; ++inv)
            for (int shift = -2; shift <= 1; ++shift)
            {
                const Voicing candidate = invert (chord, tonicRoot + wrap12 (chord.root) + 12 * shift, inv);
                const int d = distanceToHome (candidate, home);
                if (d < best)
                {
                    best = d;
                    v = candidate;
                }
            }
    }

    v.octave = std::clamp (chord.octave, -kMaxOctaveShift, kMaxOctaveShift);
    for (int i = 0; i < v.count; ++i)
        v.notes[(size_t) i] += 12 * v.octave;
    return v;
}

int bassNote (const Chord& chord, int tonic, int generalOctaves, int centreNote)
{
    const int lowestOfChord = voiceChord (chord, tonic, centreNote).notes[0] - 12 * std::clamp (chord.octave, -kMaxOctaveShift, kMaxOctaveShift);
    const int low = kBassLowest + (centreNote - kCentreNote);
    int note = low + wrap12 (lowestOfChord - low);
    note += 12 * std::clamp (chord.octave, -kMaxOctaveShift, kMaxOctaveShift);
    note += 12 * std::clamp (generalOctaves, -kMaxTotalOctave, kMaxTotalOctave);
    while (note < 0)
        note += 12;
    while (note > 127)
        note -= 12;
    return note;
}
}
