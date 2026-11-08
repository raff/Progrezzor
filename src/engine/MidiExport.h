// Copyright (C) 2026 Henrik Juhl. Part of Progrezzor, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "Arp.h"
#include "ChordRhythm.h"
#include "ChordEngine.h"
#include "Progression.h"

// What goes into the MIDI file: the notes of the progression, worked out the same way as when it plays (every chord has its
// own fixed notes, in root position unless an inversion was chosen). Pure C++, no JUCE.
namespace engine
{
constexpr int kExportVelocity = 100;
constexpr int kExportNameChords = 16; // a longer progression gets a shorter file name

struct ExportNote
{
    double start = 0.0;  // beats from the start of the file
    double length = 0.0; // beats
    int note = 60;
    int velocity = kExportVelocity;
};

// Every block strikes its chord at its start and lets go at its end, also when the next block has the same chord
// A pause, and a block that is silent (see isBlockDisabled), give a rest of their length, as it does when the loop plays.
// With `arp` on, a block gives the notes of the arpeggio instead of the chord, step by step as it plays.
// `octaveShift` (the Oct buttons): the file has the notes that are heard. With `bass` on every block has its bass note too.
inline std::vector<ExportNote> exportNotes (const Progression& progression, int tonic, ScaleId scale = ScaleId::Minor,
                                            bool lockToScale = false, int octaveShift = 0, int centreNote = kCentreNote,
                                            ArpSettings arp = {}, bool bass = false, RhythmSettings rhythm = {})
{
    std::vector<ExportNote> notes;
    double start = 0.0;
    for (const auto& block : progression)
    {
        if (block.rest || isBlockDisabled (block, scale, lockToScale)) // a pause or a silent block: a gap of its length
        {
            start += block.beats;
            continue;
        }

        const auto voicing = voiceChord (block.chord, tonic, centreNote);
        const auto heard = shiftedByOctaves (voicing, octaveShift);
        if (bass) // the bass note lies under the whole block, also when the chord is an arp
            notes.push_back ({ start, block.beats, bassNote (block.chord, tonic, octaveShift, centreNote), kExportVelocity });
        if (rhythm.on)
        {
            // Chord rhythm: the whole chord at every strike of the pattern, counted in slots from the start of the block
            for (std::int64_t step = 0; (double) step * rhythm.slotBeats() < block.beats - 1.0e-9; ++step)
            {
                if (! rhythm.isHit (step))
                    continue;
                const double at = (double) step * rhythm.slotBeats();
                const double length = std::min (rhythm.noteBeats (step), block.beats - at);
                for (int i = 0; i < heard.count; ++i)
                    notes.push_back ({ start + at, length, heard.notes[(size_t) i], rhythm.velocityFor (kExportVelocity, step) });
            }
        }
        else if (arp.on)
        {
            // The arp: the same steps as when it plays, counted from the start of the block
            const auto pattern = arp.pattern();
            const auto pool = arpPool (heard, arp.octaves);
            const double stepBeats = arp.stepBeats();
            for (std::int64_t step = 0; (double) step * stepBeats < block.beats - 1.0e-9; ++step)
            {
                const auto hit = arpStep (arp, pattern, pool, step);
                if (! hit.play)
                    continue;
                const double at = (double) step * stepBeats;
                notes.push_back ({ start + at, std::min (kArpGate * stepBeats, block.beats - at), hit.note, kExportVelocity });
            }
        }
        else
            for (int i = 0; i < heard.count; ++i)
                notes.push_back ({ start, block.beats, heard.notes[(size_t) i], kExportVelocity });
        start += block.beats;
    }
    return notes;
}

// "Am-F-C-G.mid". Only a-z, A-Z, 0-9, hyphen and underscore are used: # is written "sharp" (Csharp), and a slash (an
// inversion: Am/C) would be a folder in a file name, so it is written as an underscore (Am_C).
inline std::string exportFileName (const Progression& progression, const Key& key, bool arp = false, bool rhythm = false)
{
    std::string name;
    int count = 0;
    for (const auto& block : progression)
    {
        if (count++ == kExportNameChords)
            break;

        std::string chord;
        for (const char c : (block.rest ? std::string ("Rest") : chordName (block.chord, key)))
        {
            const bool safe = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
            if (c == '#')
                chord += "sharp";
            else
                chord += safe ? c : '_';
        }
        name += (name.empty() ? "" : "-") + chord;
    }
    return (name.empty() ? "Progression" : name) + (arp ? "-Arp" : "") + (rhythm ? "-Rhythm" : "") + ".mid";
}
}
