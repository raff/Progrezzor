// Copyright (C) 2026 Henrik Juhl. Part of Progrezzor, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <string>
#include <vector>

#include "ChordEngine.h"
#include "Progression.h"

// What goes into the MIDI file: the notes of the progression, worked out the same way as when it plays (smooth voice
// leading from block to block, the first chord in root position unless an inversion was chosen). Pure C++, no JUCE.
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
// A block that is silent (see isBlockDisabled) gives a rest of its length, as it does when the loop plays.
// `octaveShift` is the general register (the Oct buttons): the file has the notes that are heard.
inline std::vector<ExportNote> exportNotes (const Progression& progression, int tonic, ScaleId scale = ScaleId::Minor,
                                            bool lockToScale = false, int octaveShift = 0)
{
    std::vector<ExportNote> notes;
    Voicing previous;
    double start = 0.0;
    for (const auto& block : progression)
    {
        if (isBlockDisabled (block, scale, lockToScale))
        {
            start += block.beats;
            continue;
        }

        const auto voicing = voiceChord (block.chord, tonic, previous.count > 0 ? &previous : nullptr);
        previous = voicing;
        const auto heard = shiftedByOctaves (voicing, octaveShift);
        for (int i = 0; i < heard.count; ++i)
            notes.push_back ({ start, block.beats, heard.notes[(size_t) i], kExportVelocity });
        start += block.beats;
    }
    return notes;
}

// "Am-F-C-G.mid". Only a-z, A-Z, 0-9, hyphen and underscore are used: # is written "sharp" (Csharp), and a slash (an
// inversion: Am/C) would be a folder in a file name, so it is written as an underscore (Am_C).
inline std::string exportFileName (const Progression& progression, const Key& key)
{
    std::string name;
    int count = 0;
    for (const auto& block : progression)
    {
        if (count++ == kExportNameChords)
            break;

        std::string chord;
        for (const char c : chordName (block.chord, key))
        {
            const bool safe = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
            if (c == '#')
                chord += "sharp";
            else
                chord += safe ? c : '_';
        }
        name += (name.empty() ? "" : "-") + chord;
    }
    return (name.empty() ? "Progression" : name) + ".mid";
}
}
