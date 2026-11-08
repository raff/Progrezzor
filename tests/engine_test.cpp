// Copyright (C) 2026 Henrik Juhl. Part of Progrezzor, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only
//
// Unit tests of the chord engine. No JUCE, no plugin. Exit code 0 = all checks passed.

#include "../src/engine/ChordEngine.h"
#include "../src/engine/Library.h"
#include "../src/engine/LoopPlan.h"
#include "../src/engine/TripleBuffer.h"
#include "../src/engine/NoteTracker.h"
#include "../src/engine/ProgressionEdit.h"
#include "../src/engine/MidiExport.h"
#include "../src/engine/NextPads.h"
#include "../src/engine/Arp.h"
#include "../src/engine/Learn.h"
#include "../src/engine/QuickText.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <atomic>
#include <cmath>
#include <string>
#include <thread>
#include <vector>

using namespace engine;

namespace
{
int failures = 0;
int checks = 0;

void check (bool ok, const std::string& what)
{
    ++checks;
    if (! ok)
    {
        ++failures;
        std::printf ("  FAIL: %s\n", what.c_str());
    }
}

template <typename A, typename B>
void checkEqual (const A& actual, const B& expected, const std::string& what)
{
    const bool ok = actual == expected;
    check (ok, what);
    if (! ok)
        std::printf ("        got '%s', expected '%s'\n", std::string (actual).c_str(), std::string (expected).c_str());
}

std::string padNames (const Key& key)
{
    std::string result;
    for (int pad = 0; pad < 7; ++pad)
        result += (pad > 0 ? " " : "") + chordName (padChord (key.scale, pad), key);
    return result;
}

std::string pitchClasses (const Voicing& v)
{
    std::string s;
    for (int i = 0; i < v.count; ++i)
        s += (i > 0 ? " " : "") + noteName (v.notes[(size_t) i], false);
    return s;
}

bool isSortedAndInRange (const Voicing& v)
{
    if (v.count < 2 || v.notes[0] < kLowestNote || v.notes[(size_t) v.count - 1] > kHighestNote)
        return false;
    return std::is_sorted (v.notes.begin(), v.notes.begin() + v.count);
}


void testPads()
{
    std::printf ("pads\n");
    checkEqual (padNames ({ 9, ScaleId::Minor }), "Am Bdim C Dm Em F G", "A Minor pads");
    checkEqual (padNames ({ 0, ScaleId::Major }), "C Dm Em F G Am Bdim", "C Major pads");
    checkEqual (padNames ({ 2, ScaleId::Dorian }), "Dm Em F G Am Bdim C", "D Dorian pads");
    checkEqual (padNames ({ 4, ScaleId::Phrygian }), "Em F G Am Bdim C Dm", "E Phrygian pads");

    // Every pad of every key and scale fits the scale
    bool allFit = true;
    for (int s = 0; s < kNumScales; ++s)
        for (int pad = 0; pad < 7; ++pad)
            allFit &= chordFitsScale (padChord ((ScaleId) s, pad), (ScaleId) s);
    check (allFit, "all pads fit their scale");

    check (padChord (ScaleId::Minor, 7) == padChord (ScaleId::Minor, 0), "pad numbers wrap round");
}

void testNames()
{
    std::printf ("names\n");
    checkEqual (padNames ({ 5, ScaleId::Major }), "F Gm Am Bb C Dm Edim", "F Major uses Bb, not A#");
    checkEqual (padNames ({ 7, ScaleId::Major }), "G Am Bm C D Em F#dim", "G Major uses F#");
    // Every note is spelled either with flats or with sharps, never C-flat or E-sharp, so Cb is written B.
    checkEqual (padNames ({ 3, ScaleId::Minor }).substr (0, 22), "Ebm Fdim Gb Abm Bbm B ", "Eb Minor pads use flats");
}

void testNames3()
{
    check (keyUsesFlats ({ 10, ScaleId::Major }), "Bb Major uses flats");
    check (keyUsesFlats ({ 3, ScaleId::Minor }), "Eb Minor uses flats");
    check (! keyUsesFlats ({ 9, ScaleId::Minor }), "A Minor uses sharps");
    check (! keyUsesFlats ({ 6, ScaleId::Major }), "F# Major uses sharps");
    check (keyUsesFlats ({ 2, ScaleId::Minor }), "D Minor uses flats (F Major)");
    check (keyUsesFlats ({ 7, ScaleId::Dorian }) , "G Dorian uses flats (F Major)");
    check (! keyUsesFlats ({ 4, ScaleId::Phrygian }), "E Phrygian uses sharps (C Major)");
    checkEqual (keyName ({ 10, ScaleId::Minor }), "Bb", "key name Bb");

    const Chord am7 { 0, ChordType::Min7, kAutoInversion };
    checkEqual (chordName (am7, { 9, ScaleId::Minor }), "Am7", "Am7");
    checkEqual (chordName ({ 1, ChordType::Maj7, kAutoInversion }, { 5, ScaleId::Major }), "Gbmaj7", "Gbmaj7 in F Major (flat key)");
    checkEqual (chordName ({ 0, ChordType::Major, 1 }, { 0, ScaleId::Major }), "C/E", "first inversion shows the bass");
    checkEqual (chordName ({ 0, ChordType::Major, 2 }, { 0, ScaleId::Major }), "C/G", "second inversion shows the bass");
    checkEqual (chordName ({ 0, ChordType::Major, 3 }, { 0, ScaleId::Major }), "C", "inversion wraps to root position");
    checkEqual (chordName ({ 7, ChordType::Dom9, kAutoInversion }, { 0, ScaleId::Major }), "G9", "G9");
    checkEqual (chordName ({ 0, ChordType::Power5, kAutoInversion }, { 4, ScaleId::Minor }), "E5", "E5");
    checkEqual (chordName ({ 2, ChordType::Sus4, kAutoInversion }, { 0, ScaleId::Major }), "Dsus4", "Dsus4");
}

void testScaleFit()
{
    std::printf ("lock to scale\n");
    const ScaleId minor = ScaleId::Minor;
    check (chordFitsScale ({ 0, ChordType::Min7, -1 }, minor), "Am7 fits A Minor");
    check (chordFitsScale ({ 0, ChordType::Add9, -1 }, minor) == false, "Aadd9 does not fit A Minor (it has C#)");
    check (chordFitsScale ({ 0, ChordType::Minor, -1 }, minor), "Am fits");
    check (! chordFitsScale ({ 7, ChordType::Dom7, -1 }, minor), "E7 does not fit A Minor (G#)");
    check (chordFitsScale ({ 10, ChordType::Dom7, -1 }, minor), "G7 fits A Minor");
    check (chordFitsScale ({ 8, ChordType::Maj7, -1 }, minor), "Fmaj7 fits A Minor");
    check (chordFitsScale ({ 0, ChordType::Sus4, -1 }, minor), "Asus4 fits (the pure fourth D is in the scale)");
    check (chordFitsScale ({ 0, ChordType::Sus2, -1 }, minor), "Asus2 fits (B is in the scale)");
    check (! chordFitsScale ({ 2, ChordType::Sus2, -1 }, minor), "Bsus2 does not fit (C# missing)");

    // Phrygian has no major second: sus2 on the tonic is out, sus4 is in
    check (! chordFitsScale ({ 0, ChordType::Sus2, -1 }, ScaleId::Phrygian), "Phrygian: no sus2 on the tonic");
    check (chordFitsScale ({ 0, ChordType::Sus4, -1 }, ScaleId::Phrygian), "Phrygian: sus4 on the tonic");
    check (noteInScale (ScaleId::Major, 12) && noteInScale (ScaleId::Major, -12), "octaves of the tonic are in the scale");
    check (! noteInScale (ScaleId::Major, 1), "C# is not in C Major");
}

void testVoicingBasics()
{
    std::printf ("voicing\n");
    const Key aMinor { 9, ScaleId::Minor };

    for (int pad = 0; pad < 7; ++pad)
    {
        const auto v = voiceChord (padChord (aMinor.scale, pad), aMinor.tonic);
        check (isSortedAndInRange (v), "pad " + std::to_string (pad) + " is sorted and in range");
        check (v.count == 3, "pad " + std::to_string (pad) + " has three notes");
        const int lowest = v.notes[0];
        check (lowest >= 48 && lowest <= 71, "pad " + std::to_string (pad) + " sits near the centre");
    }

    // Root position, right notes
    checkEqual (pitchClasses (voiceChord (padChord (aMinor.scale, 0), aMinor.tonic)), "A C E", "Am notes");
    checkEqual (pitchClasses (voiceChord (padChord (aMinor.scale, 5), aMinor.tonic)), "F A C", "F notes");
    {
        const auto v = voiceChord (padChord (aMinor.scale, 0), aMinor.tonic);
        check (v.notes[0] % 12 == 9, "Am starts on A (root position)");
    }

    // Every chord type, in the key of every tonic, has the right notes
    bool allRight = true;
    for (int tonic = 0; tonic < 12; ++tonic)
        for (int type = 0; type < kNumChordTypes; ++type)
            for (int root = 0; root < 12; ++root)
            {
                const Chord c { root, (ChordType) type, 0 };
                const auto v = voiceChord (c, tonic);
                const auto& shape = shapeOf (c.type);
                allRight &= v.count == shape.count && isSortedAndInRange (v);
                for (int i = 0; i < v.count; ++i)
                    allRight &= wrap12 (v.notes[(size_t) i] - tonic - root) == wrap12 (shape.intervals[i]);
            }
    check (allRight, "all 12 types in all keys on all roots give the right notes in root position");

    // Moving the key moves the progression
    const Chord am { 0, ChordType::Minor, kAutoInversion };
    const auto inA = voiceChord (am, 9);
    const auto inB = voiceChord (am, 11);
    checkEqual (pitchClasses (inA), "A C E", "same chord in A");
    checkEqual (pitchClasses (inB), "B D F#", "same chord in B");
}

void testPadsClimb()
{
    std::printf ("pads climb\n");
    bool climb = true, rootLowest = true, window = true;
    for (int scaleIndex = 0; scaleIndex < kNumScales; ++scaleIndex)
        for (int tonic = 0; tonic < 12; ++tonic)
        {
            int previous = -1;
            for (int pad = 0; pad < 7; ++pad)
            {
                const Chord c = padChord ((ScaleId) scaleIndex, pad);
                const auto v = voiceChord (c, tonic);
                const int root = v.notes[0];
                climb &= root > previous;
                previous = root;
                rootLowest &= wrap12 (root - tonic - c.root) == 0;
                if (pad == 0)
                    window &= root >= 54 && root <= 65;
                window &= root >= 54 && root <= 65 + 11;
            }
        }
    check (climb, "pads 1 to 7 have rising roots in all 12 keys and all scales");
    check (rootLowest, "without an inversion the root is the lowest note");
    check (window, "the tonic sits in F#3 to F4, the others at most 11 above");

    // Chords outside the scale follow the same rule
    bool outside = true;
    for (int tonic = 0; tonic < 12; ++tonic)
        for (int root = 0; root < 12; ++root)
        {
            const auto v = voiceChord ({ root, ChordType::Major, kAutoInversion }, tonic);
            const int tonicRoot = voiceChord ({ 0, ChordType::Major, kAutoInversion }, tonic).notes[0];
            outside &= v.notes[0] == tonicRoot + root;
        }
    check (outside, "any chord's root is 0 to 11 above the tonic's, in or out of the scale");

    checkEqual (pitchClasses (voiceChord (padChord (ScaleId::Major, 0), 0)), "C E G", "C major");
    check (voiceChord (padChord (ScaleId::Major, 6), 0).notes[0] == 71, "B in C major is B4");
}

std::string namesOf (const Progression& p, const Key& key)
{
    std::string s;
    for (const auto& b : p)
        s += (s.empty() ? "" : " ") + chordName (b.chord, key);
    return s;
}

void testSuggestInversions()
{
    std::printf ("Suggest and inversions\n");
    const Key key { 0, ScaleId::Major };

    // Written out by hand: the first walking-bass entry of Bright major is C G/B Am F
    bool foundNamed = false;
    for (int i = 0; i < libraryCount (Mood::Bright, true); ++i)
        if (namesOf (libraryProgression (Mood::Bright, true, i, key), key) == "C G/B Am F")
            foundNamed = libraryEntryHasInversion (Mood::Bright, true, i);
    check (foundNamed, "the library has C G/B Am F, and it is marked as having an inversion");

    // On: some suggestions have an inversion, and it stays in the block
    int withInversion = 0;
    for (int m = 0; m < kNumMoods; ++m)
        for (int scale = 0; scale < kNumScales; ++scale)
            for (unsigned r = 1; r <= 300; ++r)
            {
                const auto s = suggest ({ 0, (ScaleId) scale }, (Mood) m, r * 2654435761u, -1, true);
                bool any = false;
                for (const auto& b : s.blocks)
                    any |= b.chord.inversion > 0;
                withInversion += any;
                if (any)
                    check (s.index >= 0 && libraryEntryHasInversion ((Mood) m, scale == (int) ScaleId::Major, s.index), "an inversion comes from the library");
            }
    check (withInversion > 0, "with inversions on, Suggest gives some of them");

    // Off: only root position, in every mood, key and scale
    bool allRoot = true;
    for (int m = 0; m < kNumMoods; ++m)
        for (int tonic = 0; tonic < 12; ++tonic)
            for (int scale = 0; scale < kNumScales; ++scale)
                for (unsigned r = 1; r <= 60; ++r)
                    for (const auto& b : suggest ({ tonic, (ScaleId) scale }, (Mood) m, r * 2654435761u, -1, false).blocks)
                        allRoot &= b.chord.inversion == kAutoInversion;
    check (allRoot, "with inversions off, Suggest gives only root positions");

    // The inverted chord has the bass note of its name, and the right notes
    const Chord gb { 7, ChordType::Major, 1 };
    const auto v = voiceChord (gb, 0);
    checkEqual (pitchClasses (v), "B D G", "G/B has B in the bass");
    check (chordName (gb, key) == "G/B", "the name has a slash: G/B");

    // A pad variant the user chose wins over the suggestion
    Progression suggestion { { gb, 4.0, false }, { Chord { 9, ChordType::Minor, kAutoInversion }, 4.0, false } };
    std::array<Chord, 7> pads;
    for (int pad = 0; pad < 7; ++pad)
        pads[(size_t) pad] = padChord (ScaleId::Major, pad);
    check (withPadVariants (suggestion, ScaleId::Major, pads)[0].chord.inversion == 1, "without a pad variant the inversion stays");
    pads[4] = Chord { 7, ChordType::Dom7, kAutoInversion }; // G changed to G7 on its pad
    const auto chosen = withPadVariants (suggestion, ScaleId::Major, pads);
    check (chosen[0].chord.type == ChordType::Dom7 && chosen[0].chord.inversion == kAutoInversion, "the pad's own G7 wins over G/B");

    // The loop and the MIDI file play the same notes as the pad would
    Progression bar { { gb, 4.0, false }, { Chord { 0, ChordType::Major, 2 }, 4.0, false } };
    const auto snapshot = makeSnapshot (bar);
    const auto notes = exportNotes (bar, key.tonic, key.scale);
    bool same = true;
    size_t at = 0;
    for (int i = 0; i < snapshot.count; ++i)
    {
        const auto heard = voiceChord (snapshot.blocks[(size_t) i].chord, key.tonic);
        for (int n = 0; n < heard.count; ++n, ++at)
            same &= at < notes.size() && notes[at].note == heard.notes[(size_t) n];
    }
    check (same && at == notes.size(), "the loop and the MIDI file have the same notes, with the inversions");
}

void testBassNote()
{
    std::printf ("bass note\n");
    bool range = true, isRoot = true, same = true, underChord = true;
    for (int scaleIndex = 0; scaleIndex < kNumScales; ++scaleIndex)
        for (int tonic = 0; tonic < 12; ++tonic)
            for (int pad = 0; pad < 7; ++pad)
            {
                const Chord c = padChord ((ScaleId) scaleIndex, pad);
                const int bass = bassNote (c, tonic);
                range &= bass >= 40 && bass <= 51;
                isRoot &= wrap12 (bass - tonic - c.root) == 0;
                same &= bass == bassNote (c, tonic);
                underChord &= bass < voiceChord (c, tonic).notes[0];
            }
    check (range, "the bass note is in E2 to D#3 for every pad in every key");
    check (isRoot, "without an inversion the bass note is the root");
    check (same, "the same pad always gets the same bass note");
    check (underChord, "the bass note is under the chord");

    // The lowest note of a chosen inversion: E for C/E, B for G/B, G for C/G, F for G7/F
    check (bassNote ({ 0, ChordType::Major, 1 }, 0) == 40, "C/E has E2 as its bass note");
    check (bassNote ({ 7, ChordType::Major, 1 }, 0) == 47, "G/B has B2 as its bass note");
    check (bassNote ({ 0, ChordType::Major, 2 }, 0) == 43, "C/G has G2 as its bass note");
    check (bassNote ({ 7, ChordType::Dom7, 3 }, 0) == 41, "G7/F has F2 as its bass note");
    bool allInversions = true;
    for (int type = 0; type < kNumChordTypes; ++type)
        for (int inversion = 0; inversion < shapeOf ((ChordType) type).count; ++inversion)
            for (int root = 0; root < 12; ++root)
            {
                const Chord c { root, (ChordType) type, inversion };
                const auto v = voiceChord (c, 3);
                allInversions &= wrap12 (bassNote (c, 3) - v.notes[0]) == 0;
            }
    check (allInversions, "the bass note is always the lowest note of the chord, whatever the inversion");

    // The Oct buttons and the chord's own octave move it like the chord
    const Chord am { 0, ChordType::Minor, kAutoInversion };
    check (bassNote (am, 9, 1) == bassNote (am, 9) + 12 && bassNote (am, 9, -2) == bassNote (am, 9) - 24, "the Oct buttons move the bass note");
    Chord up = am;
    up.octave = 1;
    check (bassNote (up, 9) == bassNote (am, 9) + 12, "so does the chord's own octave");
    check (bassNote (am, 9, -2) >= 0 && bassNote (am, 0, 4) <= 127, "it stays inside the MIDI range");

    // The chord itself does not change
    check (voiceChord (am, 9).notes == voiceChord (am, 9).notes && voiceChord (am, 9).count == 3, "the chord has the same three notes");

    // The MIDI file: the bass first in each block, as long as the block, then the chord
    const Key key { 0, ScaleId::Major };
    Progression bar { { Chord { 0, ChordType::Major, kAutoInversion }, 4.0, false }, { Chord { 7, ChordType::Major, 1 }, 2.0, false },
                      { Chord { 9, ChordType::Minor, kAutoInversion }, 4.0, false } };
    const auto off = exportNotes (bar, key.tonic, key.scale);
    const auto on = exportNotes (bar, key.tonic, key.scale, false, 0, kCentreNote, {}, true);
    check (off.size() == 9 && on.size() == 12, "the file has one more note per block with the bass note on");
    const auto snapshot = makeSnapshot (bar); // what the loop plays: the chord and the bass note of each block
    bool equal = true;
    size_t at = 0;
    double start = 0.0;
    for (int i = 0; i < snapshot.count; ++i)
    {
        const auto& block = snapshot.blocks[(size_t) i];
        const auto heard = voiceChord (block.chord, key.tonic);
        std::vector<int> expected { bassNote (block.chord, key.tonic) };
        for (int n = 0; n < heard.count; ++n)
            expected.push_back (heard.notes[(size_t) n]);
        for (int note : expected)
        {
            equal &= at < on.size() && on[at].note == note && on[at].start == start && on[at].length == block.beats;
            ++at;
        }
        start += block.beats;
    }
    check (equal && at == on.size(), "the loop and the MIDI file have the same notes, with the bass note");

    // With the arp on, the bass note still lies under the whole block while the chord is played one note at a time
    ArpSettings arp;
    arp.on = true;
    const auto withArp = exportNotes (bar, key.tonic, key.scale, false, 0, kCentreNote, arp, true);
    bool bassHeld = false;
    for (const auto& n : withArp)
        bassHeld |= n.start == 0.0 && n.length == 4.0 && n.note == bassNote (bar[0].chord, key.tonic);
    check (bassHeld && withArp.size() > on.size(), "with the arp the bass note is one long note under the arp");

    // The note tracker: an arp step lets go of its note, not of the bass note
    NoteTracker t;
    std::vector<NoteEvent> events;
    const auto emit = [&events] (const NoteEvent& e) { events.push_back (e); };
    Voicing one;
    one.count = 1;
    one.notes[0] = 60;
    t.pressBass (0, 40, 100, emit);
    t.pressNotes (0, one, 100, emit);
    one.notes[0] = 64;
    t.pressNotes (0, one, 100, emit);
    check (t.isSounding (40) && t.isSounding (64) && ! t.isSounding (60), "an arp step does not touch the bass note");
    t.release (0, emit);
    int ons = 0, offs = 0;
    for (auto& e : events)
        e.on ? ++ons : ++offs;
    check (ons == offs && t.soundingNotes() == 0, "release lets go of the bass note too: nothing hangs");
}

void testBlockMenuEdits()
{
    std::printf ("block menu edits\n");
    const Key key { 0, ScaleId::Major };
    Progression p { { Chord { 0, ChordType::Major, kAutoInversion }, 4.0, false }, { Chord { 7, ChordType::Major, kAutoInversion }, 4.0, false } };
    const auto pads = [&] { std::array<Chord, 7> a; for (int i = 0; i < 7; ++i) a[(size_t) i] = padChord (key.scale, i); return a; }();

    auto q = withChordType (p, 1, ChordType::Dom7);
    check (q[1].chord.type == ChordType::Dom7 && q[1].chord.root == 7 && q[0] == p[0], "the type changes in that block only");
    q = withInversion (q, 1, 2);
    check (q[1].chord.inversion == 2, "the inversion is set");
    q = withChordType (q, 1, ChordType::Minor);
    check (q[1].chord.inversion == kAutoInversion, "a new type starts in root position");
    q = withInversion (withInversion (q, 1, 1), 1, 0);
    check (q[1].chord.inversion == kAutoInversion, "inversion 0 is root position");
    check (pads[4] == padChord (key.scale, 4), "the pads are not touched");

    // Type, inversion, octave and length are all in the MIDI file
    q = withLength (withOctaveShift (withInversion (withChordType (p, 1, ChordType::Dom7), 1, 1), 1, 1), 1, 2.0);
    const auto notes = exportNotes (q, key.tonic, key.scale);
    const auto voiced = voiceChord (q[1].chord, key.tonic);
    std::vector<int> fromFile;
    for (const auto& n : notes)
        if (std::abs (n.start - 4.0) < 1.0e-9 && std::abs (n.length - 2.0) < 1.0e-9)
            fromFile.push_back (n.note);
    check (fromFile.size() == 4 && fromFile[0] == voiced.notes[0] && wrap12 (fromFile[0]) == 11 && voiced.octave == 1,
           "the file has G7/B: the notes, an octave up, 2 beats long");

    // A pause is left as it is
    auto r = withRestAt (p, 0);
    check (withChordType (r, 0, ChordType::Minor)[0].rest && withInversion (r, 0, 1)[0].chord == r[0].chord, "a pause has no type and no inversion");
}

void testChordRhythm()
{
    std::printf ("chord rhythm\n");
    const Key key { 0, ScaleId::Major };
    Progression one { { Chord { 0, ChordType::Major, kAutoInversion }, 4.0, false } };

    // The patterns, written out by hand: where the strikes are in one bar, in slots
    const char* eighths[kNumRhythmPatterns - 1] = { ".x.x.x.x", "x.......", "x.x.....", "..x...x.", "xxxxxxxx", "x...x..x", "x.xx.xx.", "x..x....", "x..x..x.", "x..x.x.x", "x.x.x.x." };
    bool shapesOk = true;
    for (int pattern = 0; pattern < kNumRhythmPatterns; ++pattern)
        shapesOk &= std::strlen (rhythmShapes[pattern].eighths) == 8 && std::strlen (rhythmShapes[pattern].sixteenths) == 16;
    check (shapesOk, "every pattern is written for 8 and for 16 slots");

    bool allRight = true, perBar = true, accents = true, lengths = true;
    for (int pattern = 0; pattern < kNumRhythmPatterns; ++pattern)
        for (int speed = 0; speed < 2; ++speed)
            for (int length = 0; length < 3; ++length)
            {
                RhythmSettings r;
                r.on = true;
                r.pattern = pattern;
                r.speed = speed;
                r.length = length;
                r.hits = 5;
                r.rotation = 2;
                const auto notes = exportNotes (one, key.tonic, key.scale, false, 0, kCentreNote, {}, false, r);

                // The expected strikes: from the hand-written text, and for Euclid from the arp's own rhythm engine
                std::vector<int> expectedSlots;
                const int slots = r.slots();
                for (int slot = 0; slot < slots; ++slot)
                {
                    bool hit;
                    if (pattern == (int) RhythmPattern::Euclid)
                        hit = EuclidPattern (slots, 5, 2).isOnset (slot);
                    else
                        hit = (speed == 0 ? rhythmShapes[pattern].eighths : rhythmShapes[pattern].sixteenths)[slot] == 'x';
                    if (hit)
                        expectedSlots.push_back (slot);
                }
                perBar &= notes.size() == 3 * expectedSlots.size() && ! expectedSlots.empty();
                if (pattern < kNumRhythmPatterns - 1 && speed == 0)
                {
                    std::vector<int> fromText;
                    for (int slot = 0; slot < 8; ++slot)
                        if (eighths[pattern][slot] == 'x')
                            fromText.push_back (slot);
                    allRight &= fromText == expectedSlots;
                }
                for (size_t i = 0; i < expectedSlots.size() && 3 * i + 2 < notes.size(); ++i)
                    for (size_t n = 0; n < 3; ++n)
                    {
                        const auto& note = notes[3 * i + n];
                        allRight &= std::abs (note.start - expectedSlots[i] * r.slotBeats()) < 1.0e-9;
                        accents &= note.velocity == (expectedSlots[i] == 0 ? kExportVelocity : (int) (kExportVelocity * kRhythmAccentOff + 0.5));
                        const double next = (i + 1 < expectedSlots.size() ? expectedSlots[i + 1] : expectedSlots[0] + slots) * r.slotBeats();
                        const double wanted = length == 0 ? 0.3 * r.slotBeats() : length == 1 ? 0.8 * r.slotBeats() : 0.92 * (next - expectedSlots[i] * r.slotBeats());
                        const double cut = std::min (wanted, 4.0 - expectedSlots[i] * r.slotBeats()); // the block ends after 4 beats
                        lengths &= std::abs (note.length - cut) < 1.0e-9;
                    }
            }
    check (allRight, "every pattern strikes at the slots it is written with (and Euclid as the arp's rhythm)");
    check (perBar, "every strike is the whole chord: three notes for a triad");
    check (accents, "the first slot of the bar is accented, the others a little softer");
    check (lengths, "Short, Medium and Long give the right lengths");

    // The same again gives the same: no randomness
    RhythmSettings gallop;
    gallop.on = true;
    gallop.pattern = (int) RhythmPattern::Gallop;
    gallop.speed = 1;
    check (exportNotes (one, 0, ScaleId::Major, false, 0, kCentreNote, {}, false, gallop).size() == 3 * 12, "Gallop at 1/16 has 12 strikes in a bar");
    bool same = true;
    for (int i = 0; i < 5; ++i)
    {
        const auto a = exportNotes (one, 0, ScaleId::Major, false, 0, kCentreNote, {}, false, gallop);
        const auto b = exportNotes (one, 0, ScaleId::Major, false, 0, kCentreNote, {}, false, gallop);
        for (size_t n = 0; n < a.size(); ++n)
            same &= a[n].note == b[n].note && a[n].start == b[n].start && a[n].length == b[n].length;
    }
    check (same, "no randomness: the same settings give the same strikes");

    // The pattern starts again in every block, and a block that is cut short only has the strikes before its end
    Progression two { { Chord { 0, ChordType::Major, kAutoInversion }, 2.0, false }, { Chord { 5, ChordType::Major, kAutoInversion }, 4.0, false } };
    RhythmSettings pulse;
    pulse.on = true;
    pulse.pattern = (int) RhythmPattern::Pulse;
    const auto notes = exportNotes (two, 0, ScaleId::Major, false, 0, kCentreNote, {}, false, pulse);
    int firstBlock = 0, secondBlock = 0;
    bool secondStartsOnTheBar = false;
    for (const auto& n : notes)
    {
        firstBlock += n.start < 2.0 - 1.0e-9;
        secondBlock += n.start >= 2.0 - 1.0e-9;
        secondStartsOnTheBar |= std::abs (n.start - 2.0) < 1.0e-9 && n.velocity == kExportVelocity;
    }
    check (firstBlock == 3 * 2 && secondBlock == 3 * 4 && secondStartsOnTheBar, "a half bar has the strikes before its end, and the next block starts the pattern again with the accent");

    // Euclid: Hits and Rotation
    RhythmSettings euclid;
    euclid.on = true;
    euclid.pattern = (int) RhythmPattern::Euclid;
    euclid.hits = 3;
    euclid.rotation = 0;
    bool euclidOk = euclid.isHit (0) && ! euclid.isHit (1);
    int count = 0;
    for (int slot = 0; slot < 8; ++slot)
        count += euclid.isHit (slot);
    euclid.rotation = 1;
    euclidOk &= count == 3 && ! euclid.isHit (0) && euclid.isHit (1);
    check (euclidOk, "Euclid: Hits gives that many strikes and Rotation moves them");

    // Hold is as it was: the chord as one long note
    check (exportNotes (one, 0, ScaleId::Major).size() == 3 && std::abs (exportNotes (one, 0, ScaleId::Major)[0].length - 4.0) < 1.0e-9, "Hold gives the chord as one long note");
    // The file name says Rhythm
    check (exportFileName (one, key, false, true) == "C-Rhythm.mid" && exportFileName (one, key, true, false) == "C-Arp.mid", "the file name has Rhythm, as it has Arp");
}

void testChosenInversion()
{
    std::printf ("chosen inversion\n");
    const Key c { 0, ScaleId::Major };
    const Chord first { 0, ChordType::Major, 1 };
    const Chord second { 0, ChordType::Major, 2 };

    checkEqual (pitchClasses (voiceChord (first, 0)), "E G C", "C first inversion, alone");
    checkEqual (pitchClasses (voiceChord (second, 0)), "G C E", "C second inversion, alone");

    // It is kept, whatever else was played
    const auto seventh = voiceChord ({ 7, ChordType::Dom7, 3 }, 0);
    check (seventh.notes[0] % 12 == 5, "third inversion of G7 has F in the bass");
    (void) c;
}

// No magic: a chord has the same notes every time, whatever was played before it
void testPredictableVoicing()
{
    std::printf ("predictable voicing\n");
    const Key aMinor { 9, ScaleId::Minor };

    // Written out by hand: root position, around the centre of the register
    const auto notesOf = [&] (int degree, int centre = kCentreNote)
    {
        const auto v = voiceChord (padChord (aMinor.scale, degree), aMinor.tonic, centre);
        std::string text;
        for (int i = 0; i < v.count; ++i)
            text += std::to_string (v.notes[(size_t) i]) + " ";
        return text;
    };
    checkEqual (notesOf (0), std::string ("57 60 64 "), "Am is A3 C4 E4 (57 60 64)");
    checkEqual (notesOf (5), std::string ("65 69 72 "), "F is F4 A4 C5 (65 69 72)");
    checkEqual (notesOf (2), std::string ("60 64 67 "), "C is C4 E4 G4 (60 64 67)");
    checkEqual (notesOf (6), std::string ("67 71 74 "), "G is G4 B4 D5 (67 71 74)");

    // Nothing played before has any say: a long run of random clicks, and every pad is the same notes every time
    unsigned seed = 4242;
    const auto next = [&] { seed = seed * 1664525u + 1013904223u; return (seed >> 16) & 0x7fff; };
    for (int scaleIndex = 0; scaleIndex < kNumScales; ++scaleIndex)
        for (int tonic : { 0, 3, 9, 11 })
        {
            std::array<std::string, 7> first;
            bool same = true, inBand = true;
            for (int click = 0; click < 3000; ++click)
            {
                const int pad = (int) (next() % 7);
                const auto v = voiceChord (padChord ((ScaleId) scaleIndex, pad), tonic);
                std::string text;
                for (int i = 0; i < v.count; ++i)
                    text += std::to_string (v.notes[(size_t) i]) + " ";
                inBand &= v.notes[0] >= 54 && v.notes[0] <= 65 + 11;
                if (first[(size_t) pad].empty())
                    first[(size_t) pad] = text;
                else
                    same &= first[(size_t) pad] == text;
            }
            check (same && inBand, std::string (scaleOf ((ScaleId) scaleIndex).name) + ", tonic " + std::to_string (tonic)
                                       + ": 3000 random clicks, every pad always the same notes, all around the centre");
        }

    // 20000 random chords, any type, any key: complete, sorted, playable
    unsigned seed2 = 12345;
    auto roll = [&seed2] (int n) { seed2 = seed2 * 1664525u + 1013904223u; return (int) ((seed2 >> 8) % (unsigned) n); };
    bool ok = true;
    for (int i = 0; i < 20000; ++i)
    {
        const Chord c { roll (12), (ChordType) roll (kNumChordTypes), roll (5) - 1 };
        const auto v = voiceChord (c, roll (12));
        const bool good = std::is_sorted (v.notes.begin(), v.notes.begin() + v.count) && v.count == shapeOf (c.type).count
                          && v.notes[0] >= kLowestNote - 12 && v.notes[(size_t) v.count - 1] <= kHighestNote + 12;
        if (ok && ! good)
            std::printf ("  info: root %d type %d inversion %d: %d..%d\n", c.root, (int) c.type, c.inversion, v.notes[0], v.notes[(size_t) v.count - 1]);
        ok &= good;
    }
    check (ok, "20000 random chords are all sorted, complete and within the range");
}
}

void testNoteTracker()
{
    std::printf ("note tracker\n");
    const Key key { 9, ScaleId::Minor };
    const auto am = voiceChord (padChord (key.scale, 0), key.tonic);
    const auto c = voiceChord (padChord (key.scale, 2), key.tonic);

    std::vector<NoteEvent> events;
    const auto emit = [&events] (const NoteEvent& e) { events.push_back (e); };
    const auto onsOf = [&events] { int n = 0; for (auto& e : events) n += e.on; return n; };

    NoteTracker t;
    t.press (NoteTracker::padSource (0), am, 100, emit);
    check (onsOf() == 3 && t.soundingNotes() == 3, "a chord starts three notes");

    events.clear();
    t.press (NoteTracker::padSource (2), c, 90, emit);
    int shared = 0;
    for (int i = 0; i < am.count; ++i)
        for (int j = 0; j < c.count; ++j)
            shared += am.notes[(size_t) i] == c.notes[(size_t) j];
    check (onsOf() == c.count - shared, "a note that already sounds is not started again");

    events.clear();
    t.release (NoteTracker::padSource (0), emit);
    int expectedOffs = 0;
    for (int i = 0; i < am.count; ++i)
    {
        bool inC = false;
        for (int j = 0; j < c.count; ++j)
            inC |= am.notes[(size_t) i] == c.notes[(size_t) j];
        expectedOffs += inC ? 0 : 1;
    }
    check ((int) events.size() == expectedOffs, "releasing one pad stops only the notes that the other does not hold");

    events.clear();
    t.release (NoteTracker::padSource (2), emit);
    check (t.soundingNotes() == 0, "the last release stops everything");

    events.clear();
    t.release (NoteTracker::padSource (2), emit);
    t.release (99, emit);
    t.release (-1, emit);
    check (events.empty(), "releasing what is not held does nothing");

    // Pressing a source twice lets go of the first chord first
    events.clear();
    t.press (NoteTracker::keySource (60), am, 100, emit);
    t.press (NoteTracker::keySource (60), c, 100, emit);
    t.release (NoteTracker::keySource (60), emit);
    check (t.soundingNotes() == 0, "a source pressed twice still ends silent");

    // releaseAll
    events.clear();
    t.press (NoteTracker::keySource (60), am, 100, emit);
    t.press (NoteTracker::keySource (62), c, 100, emit);
    t.press (NoteTracker::padSource (4), am, 100, emit);
    t.releaseAll (emit);
    check (t.soundingNotes() == 0, "releaseAll stops everything");
    int ons = 0, offs = 0;
    for (auto& e : events) { ons += e.on; offs += ! e.on; }
    check (ons == offs, "as many note offs as note ons");

    // The notes are released as they were started, whatever the key is now
    events.clear();
    const auto inA = voiceChord (padChord (ScaleId::Minor, 0), 9);
    t.press (0, inA, 100, emit);
    const auto inD = voiceChord (padChord (ScaleId::Major, 0), 2); // the same pad in another key
    (void) inD;
    events.clear();
    t.release (0, emit);
    bool same = events.size() == (size_t) inA.count;
    for (size_t i = 0; same && i < events.size(); ++i)
        same &= events[i].note == inA.notes[i] && ! events[i].on;
    check (same, "the note offs are exactly the notes that were started");

    // Notes outside MIDI range are never sent
    events.clear();
    Voicing wild;
    wild.count = 2;
    wild.notes = { -5, 200 };
    t.press (1, wild, 100, emit);
    check (events.empty(), "notes outside 0 to 127 are not sent");
}

void testLibraryMood (Mood mood)
{
    std::printf ("library and Suggest: %s\n", moodNames[(int) mood]);
    const std::string name = moodNames[(int) mood];

    for (bool major : { false, true })
    {
        const int count = libraryCount (mood, major);
        check (count >= 20, name + " " + (major ? "major" : "minor") + ": a library of at least 20 progressions (has " + std::to_string (count) + ")");

        std::vector<std::string> seen;
        bool allFit = true, lengthsOk = true, sizesOk = true, noEmpty = true;
        for (int i = 0; i < count; ++i)
        {
            const auto p = libraryProgression (mood, major, i, Key {});
            if (major)
                allFit &= libraryEntryFits (mood, major, i, ScaleId::Major);
            else // a minor-list progression fits at least one of Minor, Dorian and Phrygian
                allFit &= libraryEntryFits (mood, major, i, ScaleId::Minor) || libraryEntryFits (mood, major, i, ScaleId::Dorian)
                          || libraryEntryFits (mood, major, i, ScaleId::Phrygian);
            sizesOk &= p.size() >= 2 && p.size() <= 8;
            noEmpty &= ! p.empty();
            for (const auto& b : p)
                lengthsOk &= b.beats >= kMinBlockBeats && b.beats <= kMaxBlockBeats && b.chord.root >= 0 && b.chord.root < 12;

            std::string key;
            for (const auto& b : p)
                key += std::to_string (b.chord.root) + ":" + std::to_string ((int) b.chord.type) + ":" + std::to_string (b.chord.inversion) + ":" + std::to_string ((int) b.beats) + " ";
            check (std::find (seen.begin(), seen.end(), key) == seen.end(), "progression " + std::to_string (i) + " is not a copy of an earlier one");
            seen.push_back (key);
        }
        check (allFit, std::string (major ? "major" : "minor") + ": every progression fits a scale it is written for");
        check (lengthsOk && sizesOk && noEmpty, std::string (major ? "major" : "minor") + ": 2 to 8 chords, lengths within limits");
    }

    if (mood == Mood::Melancholic)
    {
        // Written out by hand: the first minor progression in A Minor and the first major in C Major
        checkEqual (namesOf (libraryProgression (mood, false, 0, Key {}), { 9, ScaleId::Minor }), "Am F C G", "first minor progression in A Minor");
        checkEqual (namesOf (libraryProgression (mood, true, 0, Key {}), { 0, ScaleId::Major }), "Am F C G", "first major progression in C Major");
        checkEqual (namesOf (libraryProgression (mood, false, 0, Key {}), { 10, ScaleId::Minor }), "Bbm Gb Db Ab", "it moves with the key (Bb Minor)");
    }

    // Suggest, for every key and scale, with many different random values
    bool allPads = true, allFits = true, nonEmpty = true, avoids = true, deterministic = true, inKeyRange = true;
    for (int tonic = 0; tonic < 12; ++tonic)
        for (int scale = 0; scale < kNumScales; ++scale)
        {
            const Key key { tonic, (ScaleId) scale };
            for (unsigned r = 0; r < 60; ++r)
            {
                const auto a = suggest (key, mood, r * 2654435761u, -1);
                const auto b = suggest (key, mood, r * 2654435761u, -1);
                nonEmpty &= ! a.blocks.empty();
                deterministic &= namesOf (a.blocks, key) == namesOf (b.blocks, key);
                for (const auto& block : a.blocks)
                {
                    allPads &= isPadChord (block.chord, key.scale);
                    allFits &= chordFitsScale (block.chord, key.scale);
                    inKeyRange &= block.chord.root >= 0 && block.chord.root < 12 && block.beats >= kMinBlockBeats;
                }
                const auto next = suggest (key, mood, r * 2654435761u, a.index);
                if (a.index >= 0)
                    avoids &= next.index != a.index || false;
            }
        }
    check (nonEmpty, "Suggest gives something in every key and scale");
    check (allFits, "everything Suggest gives fits the scale (Lock to scale is safe)");
    check (allPads, "every chord Suggest gives is one of the seven pads");
    check (inKeyRange && deterministic, "and it is predictable");
    check (avoids, "the progression from last time is not suggested again");

    for (int scale = 0; scale < kNumScales; ++scale)
    {
        int fitting = 0;
        const bool major = (ScaleId) scale == ScaleId::Major;
        for (int i = 0; i < libraryCount (mood, major); ++i)
            fitting += libraryEntryOnPads (mood, major, i, (ScaleId) scale);
        std::printf ("  info: %s: %d of %d progressions can be suggested\n", scaleOf ((ScaleId) scale).name, fitting, libraryCount (mood, major));
        check (fitting >= 5, name + " " + scaleOf ((ScaleId) scale).name + ": at least 5 library progressions can be suggested, so Suggest does not run dry");
    }

    // Variety in the usual key
    std::vector<std::string> distinct;
    for (unsigned r = 0; r < 400; ++r)
    {
        const auto s = suggest ({ 9, ScaleId::Minor }, mood, (r * 2654435761u) >> 7, -1);
        const auto n = namesOf (s.blocks, { 9, ScaleId::Minor });
        if (std::find (distinct.begin(), distinct.end(), n) == distinct.end())
            distinct.push_back (n);
    }
    check (distinct.size() >= 12, name + ": 400 suggestions in A Minor give plenty of variety (" + std::to_string (distinct.size()) + " different)");
}

void testLibrary()
{
    for (int m = 0; m < kNumMoods; ++m)
        testLibraryMood ((Mood) m);
}

Progression fourBlocks (double beats = 4.0)
{
    Progression p;
    for (int i = 0; i < 4; ++i)
        p.push_back ({ { i * 2, ChordType::Major, kAutoInversion }, beats, false });
    return p;
}

void testLoopPlan()
{
    std::printf ("loop plan\n");
    const double bps = 1.0 / 24000.0; // 120 BPM at 48 kHz: a quarter note is 24000 samples
    const auto four = makeSnapshot (fourBlocks()); // four bars of four beats: 16 beats

    {
        const auto plan = planLoop (four, 0.0, bps, 512);
        check (plan.startIndex == 0 && plan.numChanges == 0, "the start of the timeline plays block 1 from the first sample");
    }
    {
        // A block that ends 480 samples after the bar line at beat 4 (0.02 beat = 480 samples before it)
        const double start = 4.0 - 480.0 * bps;
        const auto plan = planLoop (four, start, bps, 512);
        check (plan.startIndex == 0 && plan.numChanges == 1 && plan.changes[0].sample == 480 && plan.changes[0].index == 1,
               "the change lands exactly on the bar line, in the middle of the buffer (sample 480)");
    }
    {
        const auto plan = planLoop (four, 4.0, bps, 512);
        check (plan.startIndex == 1 && plan.numChanges == 0, "a block that starts on the bar line is already in the next block");
    }
    {
        // Rounding noise just below the line must not push the change to the block before
        const auto plan = planLoop (four, 4.0 - 1.0e-12, bps, 512);
        check (plan.startIndex == 1 && plan.numChanges == 0, "noise just below the bar line counts as being on it");
    }
    {
        // The loop wraps: from the last block to the first
        const auto plan = planLoop (four, 16.0 - 100.0 * bps, bps, 512);
        check (plan.startIndex == 3 && plan.numChanges == 1 && plan.changes[0].sample == 100 && plan.changes[0].index == 0,
               "the last block is followed by the first, at the exact sample");
    }
    {
        const auto plan = planLoop (four, 16.0, bps, 512);
        check (plan.startIndex == 0, "position 16 is the start of the loop again");
        const auto later = planLoop (four, 1600.0 + 8.0, bps, 512);
        check (later.startIndex == 2, "far into the song, the position is taken modulo the length of the loop");
    }
    {
        // Count-in: before beat 0 nothing plays, and the loop starts at beat 0
        const auto plan = planLoop (four, -100.0 * bps, bps, 512);
        check (plan.startIndex == -1 && plan.numChanges == 1 && plan.changes[0].sample == 100 && plan.changes[0].index == 0,
               "before the start of the timeline the loop begins at beat 0");
        const auto before = planLoop (four, -4.0, bps, 512);
        check (before.startIndex == -1 && before.numChanges == 0, "nothing plays before the start");
    }
    {
        // Odd total lengths and half bars: 2 + 4 + 6 = 12 beats
        Progression p;
        p.push_back ({ { 0, ChordType::Minor, kAutoInversion }, 2.0, false });
        p.push_back ({ { 5, ChordType::Major, kAutoInversion }, 4.0, false });
        p.push_back ({ { 7, ChordType::Major, kAutoInversion }, 6.0, false });
        const auto s = makeSnapshot (p);
        check (planLoop (s, 1.9, bps, 512).startIndex == 0, "a half bar block ends at beat 2");
        check (planLoop (s, 2.0, bps, 512).startIndex == 1, "the second block begins at beat 2");
        check (planLoop (s, 6.0, bps, 512).startIndex == 2, "the third at beat 6");
        check (planLoop (s, 12.0, bps, 512).startIndex == 0, "and it starts over at beat 12");
        check (planLoop (s, 12.0 + 3.0, bps, 512).startIndex == 1, "second round: beat 15 is in the second block");
    }
    {
        // A huge buffer holds several changes, in order
        Progression p;
        for (int i = 0; i < 4; ++i)
            p.push_back ({ { i, ChordType::Major, kAutoInversion }, 2.0, false });
        const auto plan = planLoop (makeSnapshot (p), 0.0, bps, 24000 * 9);
        check (plan.numChanges == 4, "nine beats hold four changes when the blocks are two beats (at 2, 4, 6 and 8)");
        bool ordered = plan.numChanges > 1;
        for (int i = 1; i < plan.numChanges; ++i)
            ordered &= plan.changes[(size_t) i].sample > plan.changes[(size_t) i - 1].sample
                       && plan.changes[(size_t) i].index == (plan.changes[(size_t) i - 1].index + 1) % 4;
        check (ordered, "the changes come in order and go through the blocks one by one");
    }
    {
        // A tempo change: at 60 BPM a beat is twice as many samples
        const double slow = 1.0 / 48000.0;
        const auto plan = planLoop (four, 4.0 - 480.0 * slow, slow, 512);
        check (plan.numChanges == 1 && plan.changes[0].sample == 480, "the sample of the line follows the tempo");
    }
    {
        // Nothing to play
        LoopSnapshot empty;
        check (planLoop (empty, 0.0, bps, 512).startIndex == -1, "an empty progression plays nothing");
        check (planLoop (four, 0.0, 0.0, 512).startIndex == -1, "a tempo of zero plays nothing");
        check (planLoop (four, std::nan (""), bps, 512).startIndex == -1, "a position that is not a number plays nothing");
    }
    {
        // Walk through two loops of a song in blocks of 512 samples: every bar line is hit, one change each, on the right sample
        bool exact = true;
        int changes = 0;
        int current = 0; // the block that plays, as the host would hear it
        double position = 0.0;
        long long absolute = 0;
        for (int block = 0; block < 2 * 4 * 188; ++block)
        {
            const auto plan = planLoop (four, position, bps, 512);
            if (plan.startIndex != current) // a bar line that falls exactly on the first sample of a block
            {
                ++changes;
                exact &= absolute % 96000 == 0 && plan.startIndex == (int) ((absolute / 96000) % 4);
                current = plan.startIndex;
            }
            for (int i = 0; i < plan.numChanges; ++i)
            {
                ++changes;
                const long long at = absolute + plan.changes[(size_t) i].sample;
                exact &= at % 96000 == 0; // a bar is 96000 samples
                exact &= plan.changes[(size_t) i].index == (int) ((at / 96000) % 4);
                current = plan.changes[(size_t) i].index;
            }
            position += 512 * bps;
            absolute += 512;
        }
        check (exact, "over two rounds every change lands exactly on a bar line, on the right block");
        check (changes == 8, "and there are as many changes as bar lines (" + std::to_string (changes) + ")");
    }
}

void testTripleBuffer()
{
    std::printf ("triple buffer\n");
    TripleBuffer<int> buffer;
    check (! buffer.update(), "nothing new at first");
    buffer.writeSlot() = 7;
    buffer.publish();
    check (buffer.update() && buffer.read() == 7, "the reader gets what was published");
    check (! buffer.update() && buffer.read() == 7, "and nothing more until the writer publishes again");
    buffer.writeSlot() = 8;
    buffer.publish();
    buffer.writeSlot() = 9;
    buffer.publish();
    check (buffer.update() && buffer.read() == 9, "if the writer published twice, the reader gets the newest");

    // Two threads: the reader only ever sees whole values that were published, and never goes back in time
    struct Pair { int a = 0; int b = 0; };
    TripleBuffer<Pair> pairs;
    std::atomic<bool> stop { false };
    bool torn = false; // the reader thread only (this one)
    std::thread writer ([&]
    {
        for (int i = 1; i <= 200000; ++i)
        {
            pairs.writeSlot() = { i, i };
            pairs.publish();
        }
        stop = true;
    });
    int last = 0;
    bool backwards = false;
    while (! stop.load())
        if (pairs.update())
        {
            torn |= pairs.read().a != pairs.read().b;
            backwards |= pairs.read().a < last;
            last = pairs.read().a;
        }
    writer.join();
    if (pairs.update())
        last = pairs.read().a;
    check (! torn, "a value is never half written");
    check (! backwards, "values never come back in the wrong order");
    check (last == 200000, "the last value is always delivered");
}

// ---- Editing the progression -------------------------------------------------------------------------------------

Progression threeBlocks()
{
    Progression p;
    for (int i = 0; i < 3; ++i)
    {
        Block b;
        b.chord = { i * 2, ChordType::Minor, kAutoInversion };
        p.push_back (b);
    }
    return p;
}

void testEditing()
{
    // Lengths snap to half bars and stay between half a bar and eight bars
    check (snapBeats (4.0) == 4.0 && snapBeats (5.1) == 6.0 && snapBeats (4.9) == 4.0, "length snaps to steps of half a bar");
    check (snapBeats (0.1) == kMinBlockBeats && snapBeats (500.0) == kMaxBlockBeats, "length stays between the smallest and largest");
    check (withLength (threeBlocks(), 1, 6.1)[1].beats == 6.0, "withLength sets a snapped length");
    check (withLength (threeBlocks(), 7, 6.0).size() == 3, "withLength ignores a block that is not there");

    const auto p = threeBlocks();

    auto moved = withMoved (p, 0, 3);
    check (moved[0].chord.root == 2 && moved[1].chord.root == 4 && moved[2].chord.root == 0, "a block moves to the end");
    moved = withMoved (p, 2, 0);
    check (moved[0].chord.root == 4 && moved[1].chord.root == 0, "a block moves to the front");
    check (withMoved (p, 1, 1)[1].chord.root == 2 && withMoved (p, 1, 2)[1].chord.root == 2, "a gap next to the block changes nothing");
    check (withMoved (p, 1, 3).size() == 3, "moving never changes how many blocks there are");

    auto copy = withDuplicated (withLockToggled (p, 1), 1, 3);
    check (copy.size() == 4 && copy[3].chord == p[1].chord && ! copy[3].locked && copy[1].locked, "a copy is added and is not locked");

    auto inserted = withInserted (p, 1, { 7, ChordType::Major, kAutoInversion });
    check (inserted.size() == 4 && inserted[1].chord.root == 7 && inserted[1].beats == kDefaultBlockBeats, "insert adds one bar at the gap");
    check (inserted[2].chord == p[1].chord, "and the rest moves along");
    check (withInserted (p, 99, { 7, ChordType::Major, kAutoInversion }).back().chord.root == 7, "a far gap means the end");

    Progression full;
    for (int i = 0; i < kMaxBlocks; ++i)
        full.push_back (Block());
    check (withInserted (full, 0, {}).size() == (size_t) kMaxBlocks && withDuplicated (full, 0, 0).size() == (size_t) kMaxBlocks,
           "a full bar takes no more blocks");

    check (withErased (p, 1).size() == 2 && withErased (p, 1)[1].chord == p[2].chord, "erase removes the block");
    check (withErased (p, 5).size() == 3, "erase ignores a block that is not there");

    const Chord other { 5, ChordType::Maj7, 1 };
    const auto replaced = withReplaced (p, 2, other);
    check (replaced[2].chord == other && replaced[2].beats == p[2].beats && replaced[0].chord == p[0].chord, "replace changes only the chord");

    // Suggest keeps locked blocks
    Progression old = threeBlocks();
    old[1].locked = true;
    old[1].beats = 8.0;
    Progression fresh;
    for (int i = 0; i < 4; ++i)
    {
        Block b;
        b.chord = { 11 - i, ChordType::Major, kAutoInversion };
        fresh.push_back (b);
    }
    auto kept = keepingLocked (old, fresh);
    check (kept.size() == 4 && kept[1] == old[1] && kept[0] == fresh[0] && kept[2] == fresh[2], "a locked block stays where it was");
    old[2].locked = true;
    kept = keepingLocked (old, { fresh[0] });
    check (kept.size() == 3 && kept[1] == old[1] && kept[2] == old[2], "locked blocks past the end of a short suggestion are added");
    check (keepingLocked (threeBlocks(), fresh).size() == 4, "nothing locked: the suggestion is used as it is");

    // Suggest respects pad variants
    {
        std::array<Chord, 7> pads;
        for (int i = 0; i < 7; ++i)
            pads[(size_t) i] = padChord (ScaleId::Minor, i);
        const int bRoot = pads[6].root; // the last pad
        pads[6].type = ChordType::Dom7;

        Progression sugg;
        sugg.push_back ({ { bRoot, ChordType::Major, kAutoInversion }, 6.0, false });
        sugg.push_back ({ { pads[0].root, ChordType::Minor, kAutoInversion }, 2.0, false });
        sugg.push_back ({ { bRoot, ChordType::Major, kAutoInversion }, 4.0, true });
        const auto out = withPadVariants (sugg, ScaleId::Minor, pads);
        check (out[0].chord.type == ChordType::Dom7 && out[0].chord.root == bRoot, "a pad variant is used in the suggestion");
        check (out[1] == sugg[1], "a pad without a variant leaves its block as it is");
        check (out[2] == sugg[2], "a locked block is not touched");
        check (out[0].beats == 6.0 && out[1].beats == 2.0 && out[2].beats == 4.0, "lengths are kept");
    }
}

void testNextPads()
{
    // Every scale and every pad: at most three, all different, all real pads, never the same pad again, and at least one
    for (int s = 0; s < kNumScales; ++s)
        for (int pad = 0; pad < kScaleSize; ++pad)
        {
            const auto scale = (ScaleId) s;
            const auto next = nextPadsAfter (padChord (scale, pad), scale);
            bool sane = next.count >= 1 && next.count <= kMaxGlowingPads;
            for (int i = 0; i < next.count; ++i)
            {
                sane = sane && next.pads[(size_t) i] >= 0 && next.pads[(size_t) i] < kScaleSize && next.pads[(size_t) i] != pad;
                for (int j = i + 1; j < next.count; ++j)
                    sane = sane && next.pads[(size_t) i] != next.pads[(size_t) j];
            }
            check (sane, "the follow-ups of every pad are one to three other pads");
        }

    // A pad with another kind of chord counts as the same pad; a root outside the scale gives none
    auto variant = padChord (ScaleId::Minor, 0);
    variant.type = ChordType::Min7;
    const auto plain = nextPadsAfter (padChord (ScaleId::Minor, 0), ScaleId::Minor);
    const auto fromVariant = nextPadsAfter (variant, ScaleId::Minor);
    check (plain.count == fromVariant.count && plain.pads == fromVariant.pads, "a variant gives the same follow-ups as its pad");
    check (nextPadsAfter ({ 1, ChordType::Major, kAutoInversion }, ScaleId::Minor).count == 0, "a root outside the scale gives none");
}

void testExport()
{
    const Key aMinor { 9, ScaleId::Minor };
    Progression p;
    const auto add = [&] (int root, ChordType type, double beats, int inversion = kAutoInversion)
    {
        Block b;
        b.chord = { root, type, inversion };
        b.beats = beats;
        p.push_back (b);
    };
    add (0, ChordType::Minor, 4.0);
    add (8, ChordType::Major, 4.0);
    add (3, ChordType::Major, 2.0);
    add (10, ChordType::Major, 6.0);

    checkEqual (exportFileName (p, aMinor), std::string ("Am-F-C-G.mid"), "the file is named after the chords");
    checkEqual (exportFileName ({}, aMinor), std::string ("Progression.mid"), "an empty bar has a plain name");
    Progression inverted;
    inverted.push_back ({ { 0, ChordType::Minor, 1 }, 4.0, false });
    checkEqual (exportFileName (inverted, aMinor), std::string ("Am_C.mid"), "a slash never reaches the file name");

    // Only a-z, A-Z, 0-9, hyphen and underscore: # is "sharp", a slash is an underscore
    {
        const auto one = [] (const Chord& chord) { return Progression { { chord, 4.0, false } }; };
        checkEqual (exportFileName (one ({ 4, ChordType::Major, kAutoInversion }), { 9, ScaleId::Major }), std::string ("Csharp.mid"), "C# is written Csharp");
        checkEqual (exportFileName (one ({ 0, ChordType::Major, kAutoInversion }), { 10, ScaleId::Major }), std::string ("Bb.mid"), "Bb stays Bb");
        checkEqual (exportFileName (one ({ 0, ChordType::Minor, 1 }), { 9, ScaleId::Minor }), std::string ("Am_C.mid"), "Am/C is written Am_C");

        Progression q = one ({ 4, ChordType::Min7, kAutoInversion }); // C#m7
        q.push_back ({ { 11, ChordType::Major, kAutoInversion }, 4.0, false }); // G#
        q.push_back ({ { 0, ChordType::Minor, 2 }, 4.0, false });               // Am/E
        const auto name = exportFileName (q, { 9, ScaleId::Major });
        checkEqual (name, std::string ("Csharpm7-Gsharp-Am_E.mid"), "in a longer progression");
        bool safe = true;
        for (size_t i = 0; i + 4 < name.size(); ++i) // everything before ".mid"
            safe &= (name[i] >= 'a' && name[i] <= 'z') || (name[i] >= 'A' && name[i] <= 'Z') || (name[i] >= '0' && name[i] <= '9') || name[i] == '-' || name[i] == '_';
        check (safe, "every character before .mid is safe");
    }

    Progression many (30);
    check (exportFileName (many, aMinor).size() < 80, "a long progression gets a short name");

    const auto notes = exportNotes (p, aMinor.tonic);
    check (notes.size() == 3 + 3 + 3 + 3, "one note per chord note");
    double end = 0.0;
    bool ordered = true, inRange = true;
    for (const auto& n : notes)
    {
        end = std::max (end, n.start + n.length);
        inRange &= n.note >= kLowestNote && n.note <= kHighestNote && n.velocity > 0;
    }
    for (size_t i = 1; i < notes.size(); ++i)
        ordered &= notes[i].start >= notes[i - 1].start;
    check (end == 16.0 && ordered && inRange, "the notes fill 16 beats, in order and in range");
    check (notes[3].start == 4.0 && notes[3].length == 4.0 && notes[6].start == 8.0 && notes[6].length == 2.0, "starts and lengths follow the blocks");

    // The same as when it plays: every chord has its own fixed notes
    const auto first = voiceChord (p[0].chord, aMinor.tonic);
    const Voicing second = voiceChord (p[1].chord, aMinor.tonic);
    bool same = true;
    for (int i = 0; i < 3; ++i)
        same &= notes[(size_t) i].note == first.notes[(size_t) i] && notes[(size_t) (3 + i)].note == second.notes[(size_t) i];
    check (same, "the notes are the ones that play");
    check (exportNotes ({}, 9).empty(), "nothing to export from an empty bar");
}

void testQuickText()
{
    const Key aMinor { 9, ScaleId::Minor };
    const auto read = [&] (const std::string& text, bool lock = true) { return parseQuickText (text, aMinor, 4.0, lock); };
    const auto am = Chord { 0, ChordType::Minor, kAutoInversion };
    const auto f = Chord { 8, ChordType::Major, kAutoInversion };

    // The five forms of the CLAUDE.md
    {
        auto r = read ("Am");
        check (r.problems.empty() && r.blocks.size() == 1 && r.blocks[0].chord == am && r.blocks[0].beats == 4.0, "Am: one block of one bar");
        r = read ("Am*2");
        check (r.blocks.size() == 2 && r.blocks[0] == r.blocks[1] && r.blocks[1].beats == 4.0, "Am*2: two blocks of one bar");
        r = read ("Am:2");
        check (r.blocks.size() == 1 && r.blocks[0].beats == 8.0, "Am:2: one block of two bars");
        r = read ("Am:0.5");
        check (r.blocks.size() == 1 && r.blocks[0].beats == 2.0, "Am:0.5: one block of half a bar");
        r = read ("Am:0.5*2");
        check (r.blocks.size() == 2 && r.blocks[0].beats == 2.0 && r.blocks[1].beats == 2.0, "Am:0.5*2: two half bars");
        r = read ("Am*2:0.5");
        check (r.blocks.size() == 2 && r.blocks[0].beats == 2.0, "the order of * and : does not matter");
        r = read ("Am F, C-G");
        check (r.problems.empty() && r.blocks.size() == 4 && r.blocks[1].chord == f, "space, comma and hyphen separate chords");
    }

    // Names: the key decides the root (stored as the distance from the tonic), all the chord types, lower case roots
    {
        check (read ("C").blocks[0].chord.root == 3, "C in A minor is 3 above the tonic");
        check (read ("a").blocks[0].chord.type == ChordType::Major && read ("am").blocks[0].chord == am, "a lower case root is read");
        check (read ("Bb").blocks[0].chord.root == 1 && read ("A#").blocks[0].chord.root == 1, "Bb and A# are the same chord");
        bool allTypes = true;
        for (int t = 0; t < kNumChordTypes; ++t)
        {
            const auto r = read ("D" + std::string (chordShapes[t].suffix));
            allTypes &= r.problems.empty() && r.blocks.size() == 1 && (int) r.blocks[0].chord.type == t && r.blocks[0].chord.root == 5;
        }
        check (allTypes, "every chord type is read from the name the pads show");
        check (read ("Amin").blocks[0].chord == am && read ("Cmaj").blocks[0].chord.type == ChordType::Major, "min and maj are understood");
    }

    // Slash: a chosen inversion
    {
        const auto r = read ("Am/C");
        check (r.problems.empty() && r.blocks[0].chord.inversion == 1, "Am/C is the first inversion");
        check (read ("Am/E").blocks[0].chord.inversion == 2 && read ("Am/A").blocks[0].chord.inversion == 0, "Am/E and Am/A");
        check (read ("Am/D").problems.size() == 1 && read ("Am/D").blocks.empty(), "a bass note outside the chord is a problem");
    }

    // Problems are marked where they are, and the rest is still read
    {
        const std::string text = "Am Xq F";
        const auto r = read (text);
        check (r.problems.size() == 1 && r.problems[0].start == 3 && r.problems[0].end == 5, "the problem is at the right place");
        check (r.blocks.size() == 2 && r.blocks[0].chord == am && r.blocks[1].chord == f, "the words around it are read");
        check (read ("Am:0.3").problems.size() == 1 && read ("Am:9").problems.size() == 1 && read ("Am:0").problems.size() == 1, "lengths must be 0.5 to 8 in half steps");
        check (read ("Am*0").problems.size() == 1 && read ("Am*1.5").problems.size() == 1 && read ("Am*").problems.size() == 1, "* needs a whole number");
        check (read ("Am:").problems.size() == 1 && read ("Am:2:3").problems.size() == 1 && read ("Am*2*3").problems.size() == 1, "empty and doubled endings");
        check (read ("Am7b5").problems.size() == 1 && read ("H").problems.size() == 1, "unknown chords");
        check (read ("").blocks.empty() && read ("   ").problems.empty(), "empty text is nothing, not a problem");
    }

    // The bar holds 32 blocks
    {
        auto r = read ("Am*30 F*2");
        check (r.blocks.size() == 32 && r.problems.empty(), "32 blocks fit");
        r = read ("Am*30 F*3 C");
        check (r.blocks.size() == 31 && r.problems.size() == 1 && r.problems[0].start == 6, "a word that does not fit is a problem, the next one still fits");
    }

    // The lock: a chord outside the scale becomes a disabled block. Nothing is changed into another chord.
    {
        auto r = read ("Am A7 C");
        check (r.outsideScale == 1 && ! r.blocks[0].disabled && r.blocks[1].disabled && ! r.blocks[2].disabled, "A7 is outside A minor");
        check (r.blocks[1].chord.type == ChordType::Dom7 && r.blocks[1].chord.root == 0, "the chord is exactly what was typed");
        check (isBlockDisabled (r.blocks[1], ScaleId::Minor, true) && ! isBlockDisabled (r.blocks[1], ScaleId::Minor, false),
               "silent with the lock on, playing with it off");
        check (! isBlockDisabled (r.blocks[0], ScaleId::Minor, true), "a chord in the scale is never disabled");
        r = read ("Am A7", false);
        check (r.outsideScale == 0 && ! r.blocks[1].disabled, "with the lock off nothing is disabled");
        check (read ("A").blocks[0].disabled && ! isBlockDisabled (read ("A").blocks[0], ScaleId::Major, true), "A is outside A minor but fits A major, so it plays after the scale changes");
    }

    // Text from blocks, exactly as they are
    {
        Progression p;
        p.push_back ({ am, 4.0 });
        p.push_back ({ am, 4.0 });
        p.push_back ({ f, 8.0 });
        p.push_back ({ Chord { 3, ChordType::Major, kAutoInversion }, 2.0 });
        p.push_back ({ Chord { 0, ChordType::Min7, 1 }, 6.0 });
        checkEqual (formatQuickText (p, aMinor, 4.0), std::string ("Am Am F:2 C:0.5 Am7/C:1.5"), "the text shows every block as it is");
        const auto back = read (formatQuickText (p, aMinor, 4.0));
        check (back.problems.empty() && back.blocks.size() == p.size(), "the text can be read back");
        bool same = true;
        for (size_t i = 0; i < p.size(); ++i)
            same &= back.blocks[i].chord == p[i].chord && back.blocks[i].beats == p[i].beats;
        check (same, "and gives the same blocks");
        checkEqual (formatQuickText ({}, aMinor, 4.0), std::string(), "an empty bar is empty text");

        // Another key spells the names with flats, and reads them back
        const Key fMinor { 5, ScaleId::Minor };
        Progression q;
        q.push_back ({ Chord { 3, ChordType::Major, kAutoInversion }, 4.0 });
        checkEqual (formatQuickText (q, fMinor, 4.0), std::string ("Ab"), "named after the key");
        check (parseQuickText ("Ab", fMinor, 4.0, true).blocks[0].chord == q[0].chord, "and read after the key");

        // A bar of 3 beats
        check (parseQuickText ("Am:2", aMinor, 3.0, true).blocks[0].beats == 6.0, "a bar is what the host says");
    }

    // What the text cannot say is kept when the block is unchanged
    {
        Progression old;
        Block locked { am, 4.0 };
        locked.locked = true;
        Block rootPosition { f, 4.0 };
        rootPosition.chord.inversion = 0;
        Block outside { Chord { 0, ChordType::Dom7, kAutoInversion }, 4.0 };
        old = { locked, rootPosition, outside };

        auto read2 = keepingBlockState (old, read ("Am F A7").blocks);
        check (read2[0].locked, "the lock stays");
        check (read2[1].chord.inversion == 0, "a chosen root position stays");
        check (! read2[2].disabled, "a chord that was playing outside the scale is not silenced by an unchanged text");

        read2 = keepingBlockState (old, read ("F Am E7").blocks);
        check (! read2[1].locked && read2[0].chord.inversion == kAutoInversion, "a block that moved is a new block");
        check (read2[2].disabled, "a new chord outside the scale is disabled");
    }

    // A disabled block is a rest in the MIDI file
    {
        const auto r = read ("Am A7 C");
        const auto notes = exportNotes (r.blocks, aMinor.tonic, ScaleId::Minor, true);
        check (notes.size() == 6 && notes[3].start == 8.0, "the silent block gives no notes, and the next one starts on time");
        check (exportNotes (r.blocks, aMinor.tonic, ScaleId::Minor, false).size() == 3 + 4 + 3, "with the lock off it is in the file");
    }
}


void testOctaves()
{
    const int tonic = 9; // A
    const Chord am { 0, ChordType::Minor, kAutoInversion };
    const Chord amUp { 0, ChordType::Minor, kAutoInversion, 1 };
    const Chord amDown { 0, ChordType::Minor, kAutoInversion, -1 };

    // The same notes, a whole octave away
    const auto base = voiceChord (am, tonic);
    const auto up = voiceChord (amUp, tonic);
    const auto down = voiceChord (amDown, tonic);
    bool shifted = base.count == up.count && base.count == down.count && up.octave == 1 && down.octave == -1 && base.octave == 0;
    for (int i = 0; i < base.count; ++i)
        shifted = shifted && up.notes[(size_t) i] == base.notes[(size_t) i] + 12 && down.notes[(size_t) i] == base.notes[(size_t) i] - 12;
    check (shifted, "octave up and down move every note exactly 12 semitones");
    check (! (am == amUp) && amUp == Chord { 0, ChordType::Minor, kAutoInversion, 1 }, "the octave is part of the chord");
    checkEqual (chordName (amUp, { tonic, ScaleId::Minor }), std::string ("Am"), "the name does not change");

    // A chord that is moved changes nothing for the others
    const Chord f { 8, ChordType::Major, kAutoInversion };
    const auto fPlain = voiceChord (f, tonic);
    check (fPlain.notes == voiceChord (f, tonic).notes && fPlain.octave == 0, "the other chords are not affected by a moved chord");

    // Out of range values and every chord stay inside what MIDI can play
    bool range = voiceChord (Chord { 0, ChordType::Minor, kAutoInversion, 9 }, tonic).octave == kMaxOctaveShift;
    for (int root = 0; root < 12; ++root)
        for (int t = 0; t < kNumChordTypes; ++t)
            for (int o = -kMaxOctaveShift; o <= kMaxOctaveShift; ++o)
            {
                const auto v = voiceChord (Chord { root, (ChordType) t, kAutoInversion, o }, tonic);
                range = range && v.notes[0] >= 0 && v.notes[(size_t) v.count - 1] <= 127;
            }
    check (range, "every chord, moved the most, is a playable MIDI note");

    // Editing: one step at a time, never past the limit, other blocks and the rest of the chord stay
    Progression p;
    p.push_back ({ am, 4.0 });
    p.push_back ({ f, 4.0 });
    auto q = withOctaveShift (p, 0, 1);
    check (q[0].chord.octave == 1 && q[1].chord.octave == 0 && q[0].chord.type == ChordType::Minor, "one block is moved");
    q = withOctaveShift (withOctaveShift (withOctaveShift (q, 0, 1), 0, 1), 0, 1);
    check (q[0].chord.octave == kMaxOctaveShift, "up to the limit and no further");
    q = withOctaveShift (p, 1, -1);
    q = withOctaveShift (q, 1, -1);
    q = withOctaveShift (q, 1, -1);
    check (q[1].chord.octave == -kMaxOctaveShift && withOctaveShift (p, 5, 1) == p, "down to the limit, and a block that is not there does nothing");

    // Replacing a block takes the chord with its octave
    check (withReplaced (p, 0, amUp)[0].chord.octave == 1, "a replaced block gets the octave of the new chord");

    // The MIDI file: the notes are the ones that play
    Progression e;
    e.push_back ({ am, 4.0 });
    e.push_back ({ amUp, 4.0 });
    const auto notes = exportNotes (e, tonic);
    check (notes.size() == 6 && notes[3].note == notes[0].note + 12, "the file has the moved chord");

    // The text says how far a chord is moved: Am+1, Am-1, Am+2:2, and it comes back
    const Key key { tonic, ScaleId::Minor };
    checkEqual (formatQuickText (e, key, 4.0), std::string ("Am Am+1"), "the text shows an octave up");
    auto kept = keepingBlockState (e, parseQuickText (formatQuickText (e, key, 4.0), key, 4.0, true).blocks);
    check (kept.size() == 2 && kept[0].chord.octave == 0 && kept[1].chord.octave == 1, "an octave is read back from the text");
    const auto octaves = parseQuickText ("Am-1 F+2:2*2 C-2 G+0 Am-1:0.5 Am-F-C", key, 4.0, true);
    check (octaves.blocks.size() == 9 && octaves.blocks[0].chord.octave == -1 && octaves.blocks[1].chord.octave == 2
               && octaves.blocks[2].chord.octave == 2 && octaves.blocks[2].beats == 8.0 && octaves.blocks[3].chord.octave == -2
               && octaves.blocks[4].chord.octave == 0,
           "octaves: Am-1, F+2:2*2 (two blocks of two bars), C-2, G+0 is no move");
    check (octaves.problems.empty(), "and nothing in it is a problem");
    check (parseQuickText ("Am+3", key, 4.0, true).problems.size() == 1 && parseQuickText ("Am+", key, 4.0, true).problems.size() == 1,
           "an octave past 2 or without a number is a problem");
    check (parseQuickText ("Am-F-C-G", key, 4.0, true).blocks.size() == 4, "a hyphen still separates chords");
}

void testRomanNumerals()
{
    std::printf ("roman numerals\n");
    const auto padsAs = [] (ScaleId scale)
    {
        std::string text;
        for (int pad = 0; pad < 7; ++pad)
            text += (pad > 0 ? " " : "") + romanNumeral (padChord (scale, pad), scale);
        return text;
    };
    checkEqual (padsAs (ScaleId::Minor), std::string ("i ii\xC2\xB0 III iv v VI VII"), "Minor: i ii° III iv v VI VII");
    checkEqual (padsAs (ScaleId::Major), std::string ("I ii iii IV V vi vii\xC2\xB0"), "Major: I ii iii IV V vi vii°");
    checkEqual (padsAs (ScaleId::Dorian), std::string ("i ii III IV v vi\xC2\xB0 VII"), "Dorian: i ii III IV v vi° VII");
    checkEqual (padsAs (ScaleId::Phrygian), std::string ("i II III iv v\xC2\xB0 VI vii"), "Phrygian: i II III iv v° VI vii");

    checkEqual (romanNumeral ({ 5, ChordType::Dom7, kAutoInversion }, ScaleId::Major), std::string ("IV7"), "a seventh");
    checkEqual (romanNumeral ({ 0, ChordType::Maj7, kAutoInversion }, ScaleId::Major), std::string ("Imaj7"), "a major seventh");
    checkEqual (romanNumeral ({ 9, ChordType::Min7, kAutoInversion }, ScaleId::Major), std::string ("vi7"), "a minor seventh");
    checkEqual (romanNumeral ({ 0, ChordType::Add9, kAutoInversion }, ScaleId::Major), std::string ("Iadd9"), "add9");
    checkEqual (romanNumeral ({ 7, ChordType::Sus4, kAutoInversion }, ScaleId::Major), std::string ("Vsus4"), "sus4");
    checkEqual (romanNumeral ({ 1, ChordType::Major, kAutoInversion }, ScaleId::Major), std::string ("bII"), "outside the scale: a flat on the degree above");
    checkEqual (romanNumeral ({ 10, ChordType::Major, kAutoInversion }, ScaleId::Major), std::string ("bVII"), "bVII in a major key");
    checkEqual (romanNumeral ({ 1, ChordType::Major, kAutoInversion }, ScaleId::Minor), std::string ("bII"), "bII in a minor key");
    checkEqual (romanNumeral ({ 6, ChordType::Minor, kAutoInversion }, ScaleId::Minor), std::string ("bv"), "a minor chord outside the scale: small letters");
    checkEqual (romanNumeral ({ 4, ChordType::Major, kAutoInversion }, ScaleId::Minor), std::string ("bIV"), "between two degrees a semitone apart from both: the flat of the one above");
    checkEqual (romanNumeral ({ 11, ChordType::Major, kAutoInversion }, ScaleId::Minor), std::string ("#VII"), "the note just under the tonic is a sharp seventh, not a flat of the tonic");
}

void testLearn()
{
    std::printf ("learn mode\n");
    const int tonic = 0; // C
    Progression p;
    const Chord c { 0, ChordType::Major, kAutoInversion }, f { 5, ChordType::Major, kAutoInversion };
    const Chord am { 9, ChordType::Minor, kAutoInversion }, g { 7, ChordType::Major, kAutoInversion };
    for (const auto& chord : { c, f, am, g })
        p.push_back ({ chord, 4.0 });

    const auto classesOf = [] (std::initializer_list<int> notes)
    {
        unsigned bits = 0;
        for (int n : notes)
            bits |= 1u << (n % 12);
        return bits;
    };
    checkEqual (std::to_string (chordPitchClasses (c, tonic)), std::to_string (classesOf ({ 60, 64, 67 })), "C is C, E and G");
    checkEqual (std::to_string (chordPitchClasses (am, tonic)), std::to_string (classesOf ({ 57, 60, 64 })), "Am is A, C and E");
    checkEqual (std::to_string (chordPitchClasses (Chord { 0, ChordType::Major, kAutoInversion }, 2)), std::to_string (classesOf ({ 62, 66, 69 })), "the same chord in D is D, F# and A");

    LearnSession session;
    double now = 0.0;
    const auto hold = [&] (std::initializer_list<int> notes, double ms = 33.0)
    {
        now += ms;
        return session.update (classesOf (notes), (int) notes.size(), now, p, tonic);
    };
    const auto release = [&] { now += 33.0; return session.update (0, 0, now, p, tonic); };

    check (! session.started() && hold ({ 60 }) == LearnSession::Outcome::Nothing, "nothing happens before the session starts");
    session.start (p);
    check (session.started() && session.target() == 0 && session.blockCount() == 4 && session.playedCount() == 0, "it starts on the first block");

    // Notes come one at a time: that is not a mistake
    check (hold ({ 60 }) == LearnSession::Outcome::Nothing && hold ({ 60, 64 }) == LearnSession::Outcome::Nothing && session.tries() == 0, "the keys of a chord pressed one after the other are not wrong while the chord is not complete");
    check (hold ({ 60, 64, 67 }) == LearnSession::Outcome::Right && session.target() == 1 && session.isPlayed (0), "all three keys: the chord is right, and the next block is the target");

    // After a right chord the keys must come up first
    check (hold ({ 60, 64, 67 }) == LearnSession::Outcome::Nothing && session.target() == 1, "the same keys held on do not count for the next chord");
    release();

    // Any octave and any order is the same chord
    check (hold ({ 53, 57, 72 }) == LearnSession::Outcome::Right && session.target() == 2, "F in any octave and inversion (F A C, with the C high) is right");
    release();

    // The wrong chord, held: a try after a moment
    check (hold ({ 60, 64, 67 }) == LearnSession::Outcome::Nothing && session.tries() == 0, "the wrong chord is not wrong at the very first moment");
    check (hold ({ 60, 64, 67 }, 600.0) == LearnSession::Outcome::Wrong && session.tries() == 1 && session.target() == 2, "held for a moment it is a wrong try, and it stays on the block");
    check (hold ({ 60, 64, 67 }, 600.0) == LearnSession::Outcome::Nothing && session.tries() == 1, "and it is counted once");
    check (! session.needsHint(), "no hint after one wrong try");
    release();

    // A wrong chord that is let go quickly is a try too
    check (hold ({ 60, 63 }) == LearnSession::Outcome::Nothing, "two keys pressed");
    check (release() == LearnSession::Outcome::Wrong && session.tries() == 2 && session.needsHint(), "let go without being right: a wrong try, and the hint comes after two");

    // A right chord with an extra note is wrong
    check (hold ({ 57, 60, 64, 66 }, 700.0) == LearnSession::Outcome::Nothing, "an extra note is not the first moment");
    check (hold ({ 57, 60, 64, 66 }, 700.0) == LearnSession::Outcome::Wrong, "the right notes and one more is wrong");
    release();
    check (hold ({ 57, 60, 64 }) == LearnSession::Outcome::Right && session.target() == 3 && session.tries() == 0, "the right chord is right, and the tries start over on the next block");
    release();
    check (hold ({ 55, 59, 62 }) == LearnSession::Outcome::Finished && session.finished() && session.target() == -1 && session.playedCount() == 4, "the last chord finishes it");
    check (hold ({ 60 }) == LearnSession::Outcome::Nothing, "and nothing counts after that");

    session.start ({});
    check (! session.started(), "an empty bar has nothing to learn");
}

void testTotalOctave()
{
    std::printf ("the total octave\n");
    const Key aMinor { 9, ScaleId::Minor };
    const auto am = voiceChord (padChord (aMinor.scale, 0), aMinor.tonic);
    bool exact = true;
    for (int octaves = -kMaxTotalOctave; octaves <= kMaxTotalOctave; ++octaves)
    {
        const auto shifted = shiftedByOctaves (am, octaves);
        for (int i = 0; i < am.count; ++i)
            exact &= shifted.notes[(size_t) i] == am.notes[(size_t) i] + 12 * octaves;
    }
    check (exact, "the Oct buttons and Settings together can move the chords up to four octaves, exactly");
    const auto beyond = shiftedByOctaves (am, 9);
    check (beyond.notes[0] == am.notes[0] + 12 * kMaxTotalOctave, "and no further");
}

void testArp()
{
    std::printf ("arp\n");
    const auto rhythm = [] (int steps, int hits, int rotation)
    {
        const EuclidPattern e (steps, hits, rotation);
        std::string text;
        for (int i = 0; i < steps; ++i)
            text += e.onset[(size_t) i] ? 'x' : '.';
        return text;
    };
    checkEqual (rhythm (8, 3, 0), std::string ("x..x..x."), "E(3,8)");
    checkEqual (rhythm (8, 5, 0), std::string ("x.xx.xx."), "E(5,8)");
    checkEqual (rhythm (8, 4, 0), std::string ("x.x.x.x."), "E(4,8)");
    checkEqual (rhythm (8, 4, 1), std::string (".x.x.x.x"), "E(4,8) moved one step later is the offbeat");
    checkEqual (rhythm (8, 8, 0), std::string ("xxxxxxxx"), "all hits: every step plays");
    checkEqual (rhythm (8, 20, 0), std::string ("xxxxxxxx"), "more hits than steps: every step plays");
    checkEqual (rhythm (16, 5, 0), std::string ("x..x..x..x..x..."), "E(5,16)");

    // Which note each hit plays
    const auto order = [] (ArpStyle style, int size, int count)
    {
        std::string text;
        for (int i = 0; i < count; ++i)
            text += std::to_string (arpIndex (style, size, i));
        return text;
    };
    checkEqual (order (ArpStyle::Up, 3, 7), std::string ("0120120"), "Up");
    checkEqual (order (ArpStyle::Down, 3, 7), std::string ("2102102"), "Down");
    checkEqual (order (ArpStyle::UpDown, 3, 9), std::string ("012101210"), "Up/Down goes up and back without repeating the ends");
    checkEqual (order (ArpStyle::UpDown, 2, 6), std::string ("010101"), "Up/Down with two notes");
    checkEqual (order (ArpStyle::Up, 1, 3), std::string ("000"), "one note");
    bool inRange = true, same = true, varied = false;
    for (int i = 0; i < 200; ++i)
    {
        const int a = arpIndex (ArpStyle::Random, 5, i);
        inRange &= a >= 0 && a < 5;
        same &= a == arpIndex (ArpStyle::Random, 5, i);
        varied |= a != arpIndex (ArpStyle::Random, 5, 0);
    }
    check (inRange && same && varied, "Random is in range, the same for the same step, and not all the same note");

    // The notes of a chord, in order, for a whole run of steps
    const Key aMinor { 9, ScaleId::Minor };
    const auto am = shiftedByOctaves (voiceChord (padChord (aMinor.scale, 0), aMinor.tonic), 0);
    ArpSettings settings;
    settings.on = true;
    const auto notesFor = [&] (const ArpSettings& arp, int steps)
    {
        const auto pattern = arp.pattern();
        const auto pool = arpPool (am, arp.octaves);
        std::string text;
        for (int step = 0; step < steps; ++step)
        {
            const auto hit = arpStep (arp, pattern, pool, step);
            text += hit.play ? std::to_string (hit.note) + " " : std::string (". ");
        }
        return text;
    };
    checkEqual (notesFor (settings, 6), std::string ("57 60 64 57 60 64 "), "Am as an arp, up, every step");
    settings.style = (int) ArpStyle::Down;
    checkEqual (notesFor (settings, 4), std::string ("64 60 57 64 "), "down");
    settings.style = (int) ArpStyle::Up;
    settings.octaves = 2;
    checkEqual (notesFor (settings, 7), std::string ("57 60 64 69 72 76 57 "), "two octaves: the chord, then the same an octave higher");
    settings.octaves = 1;
    settings.hits = 3;
    checkEqual (notesFor (settings, 9), std::string ("57 . . 60 . . 64 . 57 "), "E(3,8): the rests do not use up a note");

    // The MIDI file: the same steps, from the start of every block
    Progression oneBar;
    oneBar.push_back ({ Chord { 0, ChordType::Minor, kAutoInversion }, 4.0 });
    ArpSettings fast;
    fast.on = true;
    const auto arpNotes = exportNotes (oneBar, 9, ScaleId::Minor, true, 0, kCentreNote, fast);
    const int amNotes[3] = { 57, 60, 64 };
    bool shape = arpNotes.size() == 8;
    for (size_t i = 0; shape && i < arpNotes.size(); ++i)
        shape = std::abs (arpNotes[i].start - 0.5 * (double) i) < 1.0e-9 && std::abs (arpNotes[i].length - 0.4) < 1.0e-9
                && arpNotes[i].note == amNotes[i % 3];
    check (shape, "an arp in the MIDI file: eight steps of half a beat in one bar, notes up through the chord, 80 % long");
    ArpSettings off;
    check (exportNotes (oneBar, 9, ScaleId::Minor, true, 0, kCentreNote, off).size() == 3, "with the arp off the file has the chord");

    Progression twoBlocks;
    twoBlocks.push_back ({ Chord { 0, ChordType::Minor, kAutoInversion }, 2.0 });
    twoBlocks.push_back ({ Chord { 8, ChordType::Major, kAutoInversion }, 2.0 });
    const auto two = exportNotes (twoBlocks, 9, ScaleId::Minor, true, 0, kCentreNote, fast);
    check (two.size() == 8 && two[4].start == 2.0 && two[4].note == 65, "every block starts its arp from the beginning (the second block starts on the F)");
    checkEqual (exportFileName (twoBlocks, aMinor, true), std::string ("Am-F-Arp.mid"), "the file name says Arp");
    checkEqual (exportFileName (twoBlocks, aMinor, false), std::string ("Am-F.mid"), "and does not when it is off");
}


// Pauses: a block of silence
void testRests()
{
    std::printf ("rests\n");
    const Key aMinor { 9, ScaleId::Minor };
    const Chord am { 0, ChordType::Minor, kAutoInversion };
    const Chord f { 8, ChordType::Major, kAutoInversion };

    Progression p;
    p = withInserted (p, 0, am, 4.0);
    p = withInsertedRest (p, 1, 4.0);
    p = withInserted (p, 2, f, 4.0);
    check (p.size() == 3 && ! p[0].rest && p[1].rest && ! p[2].rest && p[1].beats == 4.0, "a pause goes in like a chord, with a length");
    check (withInsertedRest (Progression (kMaxBlocks), 0).size() == (size_t) kMaxBlocks, "a full bar takes no more pauses");

    const auto replaced = withRestAt (p, 0);
    check (replaced[0].rest && replaced[0].beats == p[0].beats, "a block that is replaced by a pause keeps its length");
    check (! withReplaced (replaced, 0, f)[0].rest && withReplaced (replaced, 0, f)[0].chord == f, "a chord on a pause makes it a chord again");
    check (p[1] != p[0] && withRestAt (p, 99) == p, "a pause is not equal to a chord, and a block that is not there changes nothing");

    // The loop: the audio thread is told which block is a pause
    const auto snapshot = makeSnapshot (p);
    check (! snapshot.blocks[0].rest && snapshot.blocks[1].rest && ! snapshot.blocks[2].rest, "the loop snapshot knows the pause");

    // The MIDI file: a gap of the pause's length, and "Rest" in the name
    const auto notes = exportNotes (p, aMinor.tonic);
    check (notes.size() == 6, "the pause has no notes");
    check (notes[0].start == 0.0 && notes[3].start == 8.0, "the next chord starts after the pause");
    const auto name = exportFileName (p, aMinor);
    check (name.find ("-Rest-") != std::string::npos, "the file name says Rest");

    // Quick-text: R or Rest, a length and a number like a chord
    const auto read = [&] (const std::string& text) { return parseQuickText (text, aMinor, 4.0, true); };
    auto r = read ("Am R F");
    check (r.problems.empty() && r.blocks.size() == 3 && r.blocks[1].rest && ! r.blocks[0].rest, "R is a pause in the text");
    r = read ("Rest:2 R*2 R:0.5");
    check (r.problems.empty() && r.blocks.size() == 4 && r.blocks[0].beats == 8.0 && r.blocks[1].rest && r.blocks[2].rest && r.blocks[3].beats == 2.0,
           "Rest:2, R*2 and R:0.5 give the lengths and the numbers");
    check (r.outsideScale == 0, "a pause is never outside the scale");
    check (formatQuickText (p, aMinor) == "Am R F", "a pause is written as R");
    Progression longRest = p;
    longRest[1].beats = 8.0;
    check (formatQuickText (longRest, aMinor) == "Am R:2 F", "with its length, as for a chord");
    const auto back = read (formatQuickText (longRest, aMinor)).blocks;
    check (back.size() == 3 && back[1].rest && back[1].beats == 8.0, "and the text reads back to the same bar");
    check (! read ("Rx").problems.empty() && ! read ("Rest:9").problems.empty(), "a misspelled pause is marked, and so is a length that is too long");

    auto old = p;
    old[1].locked = true;
    check (keepingBlockState (old, read ("Am R F").blocks)[1].locked, "a pause that is unchanged keeps its lock");
    check (! keepingBlockState (old, read ("Am Am F").blocks)[1].locked && ! keepingBlockState (old, read ("Am Am F").blocks)[1].rest, "a pause changed to a chord is just a chord");

    // Learn: a pause has nothing to play and is skipped
    LearnSession session;
    session.start (p);
    check (session.target() == 0, "Learn starts on the first chord");
    const auto classes = [&] (const Chord& chord) { return chordPitchClasses (chord, aMinor.tonic); };
    session.update (classes (am), 3, 1000.0, p, aMinor.tonic);
    check (session.target() == 2 && session.isPlayed (1), "after the first chord the pause is passed and the next chord is the target");
    session.update (0, 0, 1100.0, p, aMinor.tonic);
    check (session.update (classes (f), 3, 1200.0, p, aMinor.tonic) == LearnSession::Outcome::Finished && session.finished(), "and it finishes after the last chord");
    Progression onlyRests = withInsertedRest (withInsertedRest ({}, 0), 1);
    session.start (onlyRests);
    check (session.finished() && session.target() == -1, "a bar of only pauses is finished at once");
    Progression endsWithRest = p;
    endsWithRest.pop_back();
    endsWithRest = withInsertedRest (endsWithRest, 2);
    session.start (endsWithRest);
    session.update (classes (am), 3, 2000.0, endsWithRest, aMinor.tonic);
    check (session.finished(), "a bar that ends with a pause finishes after the last chord");
}

int main()
{
    testRests();
    testLearn();
    testTotalOctave();
    testRomanNumerals();
    testArp();
    testOctaves();
    testQuickText();
    testLoopPlan();
    testEditing();
    testExport();
    testNextPads();
    testTripleBuffer();
    testLibrary();
    testNoteTracker();
    testPads();
    testNames();
    testNames3();
    testScaleFit();
    testVoicingBasics();
    testPadsClimb();
    testBassNote();
    testBlockMenuEdits();
    testChordRhythm();
    testSuggestInversions();
    testChosenInversion();
    testPredictableVoicing();

    std::printf ("%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
