// Copyright (C) 2026 Henrik Juhl. Part of Progrezzor, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <cstdlib>
#include <vector>

// The samples of the Piano preview sound: mono, 44.1 kHz, at most 3 seconds each, one for about every third note. The
// processor fills it once when the plug-in is made (never on the audio thread); after that it is only read. Pure C++.
// The notes between the samples are played by stretching the nearest one (see PadSynth).
struct PianoBank
{
    static constexpr double kSampleRate = 44100.0;

    struct Sample
    {
        int rootKey = 60;         // the MIDI note the sample was played on
        std::vector<float> data;  // mono
    };

    std::vector<Sample> samples;  // lowest root first

    // The sample whose root is nearest to `note` (the lower one if two are as near), nullptr if there are none
    const Sample* nearest (int note) const
    {
        const Sample* best = nullptr;
        for (const auto& s : samples)
            if (best == nullptr || std::abs (s.rootKey - note) < std::abs (best->rootKey - note))
                best = &s;
        return best;
    }
};
