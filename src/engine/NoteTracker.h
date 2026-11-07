// Copyright (C) 2026 Henrik Juhl. Part of Progrezzor, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <array>

#include "ChordEngine.h"

// Keeps track of which notes are sounding, so that no note ever hangs. Pure C++, no allocation, no locks: it can run on
// the audio thread.
//
// Every note has a counter. A note on is sent when the counter goes from 0 to 1, a note off when it goes back to 0, so
// when two pads share a note, it stays on until the last of them lets go. A "source" is whatever holds a chord: a pad
// that the mouse presses, or a key on a MIDI keyboard. Each source remembers exactly the notes that it started, and
// releases exactly those, also if the key or scale has changed in the meantime.
namespace engine
{
struct NoteEvent
{
    int note = 0;
    int velocity = 0;
    bool on = false;
};

class NoteTracker
{
public:
    static constexpr int kNumPadSources = 7;      // a pad held by the mouse
    static constexpr int kNumBlockSources = 32;   // a block of the progression held by the mouse
    static constexpr int kNumSources = kNumPadSources + kNumBlockSources + 1 + 128; // the loop, and a source for each MIDI key

    static constexpr int padSource (int pad) { return pad; }
    static constexpr int blockSource (int block) { return kNumPadSources + block; }
    static constexpr int loopSource() { return kNumPadSources + kNumBlockSources; }
    static constexpr int keySource (int midiNote) { return kNumPadSources + kNumBlockSources + 1 + midiNote; }

    // `emit` is called with a NoteEvent for each note that really starts or stops.
    template <typename Emit>
    void press (int source, const Voicing& voicing, int velocity, Emit&& emit)
    {
        if (source < 0 || source >= kNumSources)
            return;

        release (source, emit); // a source that is pressed twice lets go of its first chord first

        held[(size_t) source] = voicing;
        active[(size_t) source] = true;

        for (int i = 0; i < voicing.count; ++i)
        {
            const int note = voicing.notes[(size_t) i];
            if (note < 0 || note > 127)
                continue;
            if (counters[(size_t) note]++ == 0)
                emit (NoteEvent { note, velocity, true });
        }
    }

    template <typename Emit>
    void release (int source, Emit&& emit)
    {
        if (source < 0 || source >= kNumSources || ! active[(size_t) source])
            return;

        active[(size_t) source] = false;
        const auto& voicing = held[(size_t) source];
        for (int i = 0; i < voicing.count; ++i)
        {
            const int note = voicing.notes[(size_t) i];
            if (note < 0 || note > 127)
                continue;
            if (--counters[(size_t) note] == 0)
                emit (NoteEvent { note, 0, false });
        }
    }

    // Transport stop, bypass, "all notes off": every source lets go
    template <typename Emit>
    void releaseAll (Emit&& emit)
    {
        for (int source = 0; source < kNumSources; ++source)
            release (source, emit);
    }

    bool isSounding (int note) const { return note >= 0 && note < 128 && counters[(size_t) note] > 0; }
    bool isHeld (int source) const { return source >= 0 && source < kNumSources && active[(size_t) source]; }

    int soundingNotes() const
    {
        int count = 0;
        for (int c : counters)
            count += c > 0 ? 1 : 0;
        return count;
    }

private:
    std::array<int, 128> counters {};
    std::array<Voicing, kNumSources> held {};
    std::array<bool, kNumSources> active {};
};
}
