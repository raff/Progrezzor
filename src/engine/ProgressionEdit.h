// Copyright (C) 2026 Henrik Juhl. Part of Progrezzor, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "Progression.h"

// Editing the progression. Pure C++, no JUCE. Every function takes a progression and returns a new one, so that undo is
// just "keep the old one". Nothing here changes a chord into another chord by itself.
//
// A "gap" is a place between blocks: gap 0 is before the first block, gap n is after the last one.
namespace engine
{
// A length snapped to whole steps of the smallest length, and kept between the smallest and the largest
inline double snapBeats (double beats)
{
    const double steps = std::round (beats / kMinBlockBeats);
    return std::clamp (steps * kMinBlockBeats, kMinBlockBeats, kMaxBlockBeats);
}

inline Progression withLength (Progression p, int index, double beats)
{
    if (index >= 0 && index < (int) p.size())
        p[(size_t) index].beats = snapBeats (beats);
    return p;
}

inline Progression withReplaced (Progression p, int index, const Chord& chord)
{
    if (index >= 0 && index < (int) p.size())
    {
        p[(size_t) index].chord = chord;
        p[(size_t) index].disabled = false; // a chord the user picks on a pad plays
        p[(size_t) index].rest = false;     // and it is no longer a pause
    }
    return p;
}

// The block's own chord type (from the block menu): root position, the same octave. A pad is not touched, and a chord that
// the user picks is no longer silent.
inline Progression withChordType (Progression p, int index, ChordType type)
{
    if (index >= 0 && index < (int) p.size() && ! p[(size_t) index].rest)
    {
        auto& block = p[(size_t) index];
        block.chord.type = type;
        block.chord.inversion = kAutoInversion;
        block.disabled = false;
    }
    return p;
}

// The block's inversion (0 or kAutoInversion: root position). A number past the last note wraps round, as in voiceChord.
inline Progression withInversion (Progression p, int index, int inversion)
{
    if (index >= 0 && index < (int) p.size() && ! p[(size_t) index].rest)
        p[(size_t) index].chord.inversion = inversion <= 0 ? kAutoInversion : inversion;
    return p;
}

// The block becomes a pause (it keeps its length and its lock)
inline Progression withRestAt (Progression p, int index)
{
    if (index >= 0 && index < (int) p.size())
    {
        p[(size_t) index].chord = Chord {};
        p[(size_t) index].disabled = false;
        p[(size_t) index].rest = true;
    }
    return p;
}

// The block's chord moved `steps` octaves (+1 up, -1 down), never past kMaxOctaveShift either way
inline Progression withOctaveShift (Progression p, int index, int steps)
{
    if (index >= 0 && index < (int) p.size())
        p[(size_t) index].chord.octave = std::clamp (p[(size_t) index].chord.octave + steps, -kMaxOctaveShift, kMaxOctaveShift);
    return p;
}

inline Progression withLockToggled (Progression p, int index)
{
    if (index >= 0 && index < (int) p.size())
        p[(size_t) index].locked = ! p[(size_t) index].locked;
    return p;
}

inline Progression withErased (Progression p, int index)
{
    if (index >= 0 && index < (int) p.size())
        p.erase (p.begin() + index);
    return p;
}

// A new block with the chord at the gap. Full bar: nothing happens.
inline Progression withInserted (Progression p, int gap, const Chord& chord, double beats = kDefaultBlockBeats)
{
    if ((int) p.size() >= kMaxBlocks)
        return p;

    Block block;
    block.chord = chord;
    block.beats = std::clamp (beats, kMinBlockBeats, kMaxBlockBeats);
    p.insert (p.begin() + std::clamp (gap, 0, (int) p.size()), block);
    return p;
}

// A pause at the gap (a block of silence)
inline Progression withInsertedRest (Progression p, int gap, double beats = kDefaultBlockBeats)
{
    const auto before = (int) p.size();
    p = withInserted (std::move (p), gap, Chord {}, beats);
    if ((int) p.size() > before)
        p[(size_t) std::clamp (gap, 0, before)].rest = true;
    return p;
}

// The block `from` moves to the gap (a gap right before or right after the block itself changes nothing)
inline Progression withMoved (Progression p, int from, int gap)
{
    if (from < 0 || from >= (int) p.size())
        return p;

    gap = std::clamp (gap, 0, (int) p.size());
    const Block block = p[(size_t) from];
    p.erase (p.begin() + from);
    if (gap > from)
        --gap;
    p.insert (p.begin() + gap, block);
    return p;
}

// A copy of the block `from` at the gap. The copy is not locked. Full bar: nothing happens.
inline Progression withDuplicated (Progression p, int from, int gap)
{
    if (from < 0 || from >= (int) p.size() || (int) p.size() >= kMaxBlocks)
        return p;

    Block copy = p[(size_t) from];
    copy.locked = false;
    copy.id = 0; // a new block, with an identity of its own
    p.insert (p.begin() + std::clamp (gap, 0, (int) p.size()), copy);
    return p;
}

// A suggestion that respects the pad variants the user has chosen: when a block's root matches a pad that plays another
// kind of chord than the scale gives it (B changed to B7), the block gets that pad's type and inversion. Lengths, blocks
// without a matching variant pad and locked blocks are left as they are. Variants outside the scale are used too.
inline Progression withPadVariants (Progression suggestion, ScaleId scale, const std::array<Chord, 7>& pads)
{
    for (auto& block : suggestion)
    {
        if (block.locked)
            continue;

        for (int pad = 0; pad < 7; ++pad)
        {
            const auto plain = padChord (scale, pad);
            const auto& current = pads[(size_t) pad];
            if (current.root == block.chord.root && (current.type != plain.type || current.inversion != plain.inversion))
            {
                block.chord.type = current.type;
                block.chord.inversion = current.inversion;
                break;
            }
        }
    }
    return suggestion;
}

// A fresh suggestion where the locked blocks of the old progression stay (same place, same length). A locked block past
// the end of the suggestion is added at the end.
inline Progression keepingLocked (const Progression& old, Progression suggestion)
{
    for (size_t i = 0; i < old.size(); ++i)
    {
        if (! old[i].locked)
            continue;

        if (i < suggestion.size())
            suggestion[i] = old[i];
        else if ((int) suggestion.size() < kMaxBlocks)
            suggestion.push_back (old[i]);
    }
    return suggestion;
}
}
