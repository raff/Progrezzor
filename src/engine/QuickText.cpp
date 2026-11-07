// Copyright (C) 2026 Henrik Juhl. Part of Progrezzor, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only
#include "QuickText.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace engine
{
namespace
{
bool isSpace (char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ','; }

// A space or a comma separates chords, and so does a hyphen, except when a number follows it right away (Am-1: an octave down)
bool isSeparatorAt (const std::string& text, size_t i)
{
    if (isSpace (text[i]))
        return true;
    return text[i] == '-' && ! (i > 0 && ! isSpace (text[i - 1]) && text[i - 1] != '-' && i + 1 < text.size() && text[i + 1] >= '0' && text[i + 1] <= '9');
}

// A to G (also a to g), then # or b. Gives the pitch class 0 to 11, or -1. `pos` moves past what was read.
int readNote (const std::string& word, size_t& pos)
{
    if (pos >= word.size())
        return -1;

    int letter = word[pos];
    if (letter >= 'a' && letter <= 'g')
        letter = letter - 'a' + 'A';

    int pitch = -1;
    switch (letter)
    {
        case 'C': pitch = 0; break;
        case 'D': pitch = 2; break;
        case 'E': pitch = 4; break;
        case 'F': pitch = 5; break;
        case 'G': pitch = 7; break;
        case 'A': pitch = 9; break;
        case 'B': pitch = 11; break;
        default: return -1;
    }
    ++pos;

    if (pos < word.size() && word[pos] == '#')
    {
        ++pitch;
        ++pos;
    }
    else if (pos < word.size() && word[pos] == 'b')
    {
        --pitch;
        ++pos;
    }
    return wrap12 (pitch);
}

// The spelling of a chord type in the text; a few common alternatives are understood too
bool readType (const std::string& suffix, ChordType& type)
{
    for (int t = 0; t < kNumChordTypes; ++t)
        if (suffix == chordShapes[t].suffix)
        {
            type = (ChordType) t;
            return true;
        }

    static const struct { const char* text; ChordType type; } alternatives[] = {
        { "maj", ChordType::Major }, { "min", ChordType::Minor }, { "min7", ChordType::Min7 }, { "min9", ChordType::Min9 },
    };
    for (const auto& a : alternatives)
        if (suffix == a.text)
        {
            type = a.type;
            return true;
        }
    return false;
}

bool readNumber (const std::string& text, double& value)
{
    if (text.empty())
        return false;

    int dots = 0;
    for (const char c : text)
    {
        if (c == '.')
            ++dots;
        else if (c < '0' || c > '9')
            return false;
    }
    if (dots > 1 || text == ".")
        return false;

    value = std::atof (text.c_str());
    return true;
}

std::string quote (const std::string& word) { return "\"" + word + "\""; }

// Reads one word. Gives the block (or an empty message and a problem text) and how many blocks it stands for.
struct Word
{
    Block block;
    int count = 1;
    std::string problem; // empty if the word was read
};

Word readWord (const std::string& word, double barBeats)
{
    Word out;
    size_t pos = 0;

    const int rootPitch = readNote (word, pos);
    if (rootPitch < 0)
    {
        out.problem = quote (word) + " is not a chord";
        return out;
    }

    // The kind of chord: everything up to / + - : or *
    const size_t typeStart = pos;
    while (pos < word.size() && word[pos] != '/' && word[pos] != ':' && word[pos] != '*' && word[pos] != '+' && word[pos] != '-')
        ++pos;
    ChordType type = ChordType::Major;
    if (! readType (word.substr (typeStart, pos - typeStart), type))
    {
        out.problem = quote (word) + " is not a chord I know";
        return out;
    }

    // Stored as the distance from the tonic, so the caller has to subtract the tonic: the root is kept as a pitch class here
    out.block.chord = { rootPitch, type, kAutoInversion };

    int bassPitch = -1;
    if (pos < word.size() && word[pos] == '/')
    {
        ++pos;
        bassPitch = readNote (word, pos);
        if (bassPitch < 0)
        {
            out.problem = quote (word) + ": a note is missing after /";
            return out;
        }
        const auto& shape = shapeOf (type);
        int index = -1;
        for (int i = 0; i < shape.count; ++i)
            if (wrap12 (rootPitch + shape.intervals[i]) == bassPitch)
                index = i;
        if (index < 0)
        {
            out.problem = quote (word) + ": that note is not in the chord";
            return out;
        }
        out.block.chord.inversion = index;
    }

    // An octave: +1, -1, +2 or -2 (how far the chord is moved), right after the chord
    if (pos < word.size() && (word[pos] == '+' || word[pos] == '-'))
    {
        const int direction = word[pos] == '+' ? 1 : -1;
        ++pos;
        const size_t digitsStart = pos;
        while (pos < word.size() && word[pos] >= '0' && word[pos] <= '9')
            ++pos;
        const std::string digits = word.substr (digitsStart, pos - digitsStart);
        if (digits.empty() || digits.size() > 1 || digits[0] - '0' > kMaxOctaveShift)
        {
            out.problem = quote (word) + ": the octave is +1 or +2 up, -1 or -2 down";
            return out;
        }
        out.block.chord.octave = direction * (digits[0] - '0');
    }

    bool haveLength = false, haveCount = false;
    double bars = 1.0;
    while (pos < word.size())
    {
        const char sign = word[pos++];
        if (sign != ':' && sign != '*')
        {
            out.problem = quote (word) + ": unexpected " + std::string (1, word[pos - 1]);
            return out;
        }

        const size_t numberStart = pos;
        while (pos < word.size() && word[pos] != ':' && word[pos] != '*')
            ++pos;
        double number = 0.0;
        if (! readNumber (word.substr (numberStart, pos - numberStart), number))
        {
            out.problem = quote (word) + (sign == ':' ? ": a length is missing after :" : ": a number is missing after *");
            return out;
        }

        if (sign == ':')
        {
            if (haveLength || number < kMinQuickBars || number > kMaxQuickBars || std::abs (number * 2.0 - std::round (number * 2.0)) > 1.0e-9)
            {
                out.problem = quote (word) + ": the length is 0.5 to 8 bars, in steps of 0.5";
                return out;
            }
            haveLength = true;
            bars = number;
        }
        else
        {
            if (haveCount || number < 1.0 || number > (double) kMaxBlocks || std::abs (number - std::round (number)) > 1.0e-9)
            {
                out.problem = quote (word) + ": * needs a whole number from 1 to " + std::to_string (kMaxBlocks);
                return out;
            }
            haveCount = true;
            out.count = (int) std::round (number);
        }
    }

    out.block.beats = std::clamp (bars * barBeats, kMinBlockBeats, kMaxBlockBeats);
    return out;
}

// 0.5, 1.5, 2: no zeros at the end
std::string formatBars (double bars)
{
    char text[32];
    std::snprintf (text, sizeof (text), "%.2f", bars);
    std::string s = text;
    while (! s.empty() && s.back() == '0')
        s.pop_back();
    if (! s.empty() && s.back() == '.')
        s.pop_back();
    return s;
}
}

std::string formatQuickText (const Progression& progression, const Key& key, double barBeats)
{
    std::string text;
    for (const auto& block : progression)
    {
        if (! text.empty())
            text += ' ';
        text += chordName (block.chord, key);
        if (block.chord.octave != 0)
            text += (block.chord.octave > 0 ? "+" : "-") + std::to_string (std::abs (block.chord.octave));
        const double bars = block.beats / barBeats;
        if (std::abs (bars - 1.0) > 0.005)
            text += ":" + formatBars (bars);
    }
    return text;
}

QuickTextResult parseQuickText (const std::string& text, const Key& key, double barBeats, bool lockToScale)
{
    QuickTextResult result;
    const int length = (int) text.size();
    int i = 0;
    while (i < length)
    {
        if (isSeparatorAt (text, (size_t) i))
        {
            ++i;
            continue;
        }

        int end = i;
        while (end < length && ! isSeparatorAt (text, (size_t) end))
            ++end;
        const std::string word = text.substr ((size_t) i, (size_t) (end - i));

        auto read = readWord (word, barBeats);
        if (read.problem.empty() && (int) result.blocks.size() + read.count > kMaxBlocks)
            read.problem = "No room for " + quote (word) + ": the bar holds " + std::to_string (kMaxBlocks) + " chords";

        if (! read.problem.empty())
            result.problems.push_back ({ i, end, read.problem });
        else
        {
            read.block.chord.root = wrap12 (read.block.chord.root - key.tonic);
            const bool outside = lockToScale && ! chordFitsScale (read.block.chord, key.scale);
            read.block.disabled = outside;
            for (int n = 0; n < read.count; ++n)
            {
                result.blocks.push_back (read.block);
                if (outside)
                    ++result.outsideScale;
            }
        }
        i = end;
    }
    return result;
}

Progression keepingBlockState (const Progression& old, Progression read)
{
    for (size_t i = 0; i < read.size() && i < old.size(); ++i)
    {
        auto& block = read[i];
        const auto& before = old[i];
        if (block.chord.root != before.chord.root || block.chord.type != before.chord.type)
            continue;

        // The text says "Am" for both an automatic inversion and a chosen root position
        if (block.chord.inversion == kAutoInversion && before.chord.inversion == 0)
            block.chord.inversion = 0;

        if (block.chord == before.chord)
        {
            block.locked = before.locked;
            block.disabled = before.disabled;
        }
    }
    return read;
}
}
