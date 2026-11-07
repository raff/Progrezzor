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

namespace
{
// Notes of the chord in the given inversion, ascending, around the octave where the tonic is 0..11.
Voicing invert (const Chord& chord, int tonic, int inversion)
{
    const auto& shape = shapeOf (chord.type);
    Voicing v;
    v.count = shape.count;
    for (int i = 0; i < shape.count; ++i)
        v.notes[(size_t) i] = wrap12 (tonic + chord.root) + shape.intervals[i] + (i < inversion ? 12 : 0);
    // The raised notes may now pass others (the root raised an octave is above a seventh, but below a ninth), so sort.
    // A plain insertion sort: five notes at most, and no warnings from the compiler about the array bounds.
    for (int i = 1; i < v.count; ++i)
        for (int j = i; j > 0 && v.notes[(size_t) j - 1] > v.notes[(size_t) j]; --j)
            std::swap (v.notes[(size_t) j - 1], v.notes[(size_t) j]);
    return v;
}

double meanOf (const Voicing& v)
{
    double sum = 0.0;
    for (int i = 0; i < v.count; ++i)
        sum += v.notes[(size_t) i];
    return sum / v.count;
}

// How far the voices have to move: each new note to the nearest note that sounded before. A shared note costs nothing.
double movement (const Voicing& from, const Voicing& to)
{
    double total = 0.0;
    for (int i = 0; i < to.count; ++i)
    {
        int nearest = 1000;
        for (int j = 0; j < from.count; ++j)
            nearest = std::min (nearest, std::abs (to.notes[(size_t) i] - from.notes[(size_t) j]));
        total += nearest;
    }
    return total;
}
}

Voicing shiftedByOctaves (Voicing voicing, int octaves)
{
    octaves = std::clamp (octaves, -kMaxGeneralOctave, kMaxGeneralOctave);
    // Fewer octaves if the top or the bottom would leave the MIDI range
    while (octaves > 0 && voicing.count > 0 && voicing.notes[(size_t) voicing.count - 1] + 12 * octaves > 127)
        --octaves;
    while (octaves < 0 && voicing.count > 0 && voicing.notes[0] + 12 * octaves < 0)
        ++octaves;
    for (int i = 0; i < voicing.count; ++i)
        voicing.notes[(size_t) i] += 12 * octaves;
    return voicing;
}

Voicing voiceChord (const Chord& chord, int tonic, const Voicing* previous)
{
    // The voice leading works in the unmoved register: the chord that sounded before is taken back to where it was
    Voicing unmoved;
    if (previous != nullptr && previous->octave != 0)
    {
        unmoved = *previous;
        for (int i = 0; i < unmoved.count; ++i)
            unmoved.notes[(size_t) i] -= 12 * unmoved.octave;
        unmoved.octave = 0;
        previous = &unmoved;
    }

    const int count = shapeOf (chord.type).count;
    const bool autoInversion = chord.inversion < 0;
    const int firstInversion = autoInversion ? 0 : chord.inversion % count;
    const bool haveMovement = previous != nullptr && previous->count > 0;
    // Alone, a chord is in root position. After another chord, any inversion may be the smoothest.
    const int lastInversion = autoInversion ? (haveMovement ? count - 1 : 0) : firstInversion;

    Voicing best;
    double bestCost = 1.0e9;

    for (int inversion = firstInversion; inversion <= lastInversion; ++inversion)
    {
        const Voicing base = invert (chord, tonic, inversion);

        for (int octave = 0; octave < 10; ++octave)
        {
            Voicing candidate = base;
            for (int i = 0; i < candidate.count; ++i)
                candidate.notes[(size_t) i] += 12 * octave;

            if (candidate.notes[0] < kLowestNote || candidate.notes[(size_t) candidate.count - 1] > kHighestNote)
                continue;

            const double distanceFromCentre = std::abs (meanOf (candidate) - kCentreNote);
            // A small cost for each inversion, so that root position wins when nothing else decides.
            double cost = 0.25 * distanceFromCentre + (autoInversion ? 1.5 * inversion : 0.0);
            if (haveMovement)
                cost += movement (*previous, candidate);
            else
                cost += 0.75 * distanceFromCentre;

            if (cost < bestCost)
            {
                bestCost = cost;
                best = candidate;
            }
        }
    }

    best.octave = std::clamp (chord.octave, -kMaxOctaveShift, kMaxOctaveShift);
    for (int i = 0; i < best.count; ++i)
        best.notes[(size_t) i] += 12 * best.octave;
    return best;
}
}
