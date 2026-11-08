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
    // press and release work on everything a source holds: its chord and its bass note.
    template <typename Emit>
    void press (int source, const Voicing& voicing, int velocity, Emit&& emit)
    {
        if (source < 0 || source >= kNumSources)
            return;

        release (source, emit); // a source that is pressed twice lets go of its first chord first
        pressNotes (source, voicing, velocity, emit);
    }

    template <typename Emit>
    void release (int source, Emit&& emit)
    {
        releaseNotes (source, emit);
        releaseBass (source, emit);
    }

    // Only the notes of the chord (an arp step), the bass note of the source is left as it is
    template <typename Emit>
    void pressNotes (int source, const Voicing& voicing, int velocity, Emit&& emit)
    {
        if (source < 0 || source >= kNumSources)
            return;

        releaseNotes (source, emit);
        held[(size_t) source] = voicing;
        active[(size_t) source] = true;

        for (int i = 0; i < voicing.count; ++i)
            start (voicing.notes[(size_t) i], velocity, emit);
    }

    template <typename Emit>
    void releaseNotes (int source, Emit&& emit)
    {
        if (source < 0 || source >= kNumSources || ! active[(size_t) source])
            return;

        active[(size_t) source] = false;
        const auto& voicing = held[(size_t) source];
        for (int i = 0; i < voicing.count; ++i)
            stop (voicing.notes[(size_t) i], emit);
    }

    // The bass note: one note under the chord that lasts as long as the source is held, also while the chord is an arp
    template <typename Emit>
    void pressBass (int source, int note, int velocity, Emit&& emit)
    {
        if (source < 0 || source >= kNumSources)
            return;

        releaseBass (source, emit);
        bassHeld[(size_t) source] = note;
        start (note, velocity, emit);
    }

    template <typename Emit>
    void releaseBass (int source, Emit&& emit)
    {
        if (source < 0 || source >= kNumSources || bassHeld[(size_t) source] < 0)
            return;

        const int note = bassHeld[(size_t) source];
        bassHeld[(size_t) source] = -1;
        stop (note, emit);
    }

    // Transport stop, bypass, "all notes off": every source lets go
    template <typename Emit>
    void releaseAll (Emit&& emit)
    {
        for (int source = 0; source < kNumSources; ++source)
            release (source, emit);
    }

    bool isSounding (int note) const { return note >= 0 && note < 128 && counters[(size_t) note] > 0; }
    bool isHeld (int source) const { return source >= 0 && source < kNumSources && (active[(size_t) source] || bassHeld[(size_t) source] >= 0); }

    int soundingNotes() const
    {
        int count = 0;
        for (int c : counters)
            count += c > 0 ? 1 : 0;
        return count;
    }

private:
    template <typename Emit>
    void start (int note, int velocity, Emit& emit)
    {
        if (note < 0 || note > 127)
            return;
        if (counters[(size_t) note]++ == 0)
            emit (NoteEvent { note, velocity, true });
    }

    template <typename Emit>
    void stop (int note, Emit& emit)
    {
        if (note < 0 || note > 127)
            return;
        if (--counters[(size_t) note] == 0)
            emit (NoteEvent { note, 0, false });
    }

    std::array<int, 128> counters {};
    std::array<Voicing, kNumSources> held {};
    std::array<bool, kNumSources> active {};
    std::array<int, kNumSources> bassHeld = [] { std::array<int, kNumSources> a {}; a.fill (-1); return a; }(); // the bass note of a source, -1 if none
};
}
