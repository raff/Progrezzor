// Copyright (C) 2026 Henrik Juhl. Part of Progrezzor, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <string>
#include <vector>

#include "Progression.h"

// Quick-text: the progression as one line of text, and the line read back. Pure C++, no JUCE.
//
//   Am        one block of one bar
//   Am*2      two blocks of one bar (a shortcut for Am Am)
//   Am:2      one block of two bars
//   Am:0.5    one block of half a bar
//   Am:0.5*2  two blocks of half a bar
//   Am/C      Am with C in the bass (an inversion that is chosen, so it is kept)
//   Am+1      Am an octave up (+1, +2) or down (-1, -2); it comes right after the chord, before : and *: Am-1:2*2
//
// Bars: with a | in the text the chord chart is read bar by bar, and the line breaks and extra spaces do not matter:
//   | C | Am | F | G |   every bar is one block of one bar
//   | C G |              two chords in a bar share it: two blocks of half a bar (a bar holds two at the most)
//   | C | % | F | / |     % or / is the chord before again
//   | C:2 | G |           a word with its own length keeps it
// The notation above works inside the bars too (| Am*2 | is two half bars).
//
// R (or Rest) is a pause: R, R:2, Rest*2. * means how many, : means how long (in bars). Chords are separated by a space, a comma or a hyphen. A word that cannot be
// read is reported (where it is and what is wrong) and the rest is read as usual. Nothing is ever corrected by itself.
namespace engine
{
constexpr double kMinQuickBars = 0.5;
constexpr double kMaxQuickBars = 8.0;

// A word of the text that cannot be read: [start, end) are character positions in the text
struct QuickTextProblem
{
    int start = 0;
    int end = 0;
    std::string message;
};

struct QuickTextResult
{
    Progression blocks;                     // everything that could be read
    std::vector<QuickTextProblem> problems; // everything that could not
    int outsideScale = 0;                   // blocks marked as disabled because the chord is not in the scale
};

// The blocks as text, exactly as they are: Am, Am:2, Am:0.5, and R for a pause. Two blocks that are alike stay two words (Am Am).
std::string formatQuickText (const Progression&, const Key&, double barBeats = kBeatsPerBar);

// Reads the text. With the lock on, a chord that is not in the scale becomes a disabled block (see isBlockDisabled).
QuickTextResult parseQuickText (const std::string& text, const Key&, double barBeats, bool lockToScale);

// What was read becomes the new progression, but a block that is unchanged at the same place keeps what the text cannot say:
// its lock, a chosen root position, and whether it was disabled. (The octave is in the text.)
Progression keepingBlockState (const Progression& old, Progression read);
}
