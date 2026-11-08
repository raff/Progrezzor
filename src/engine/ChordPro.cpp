// Copyright (C) 2026 Henrik Juhl. Part of Progrezzor, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only
#include "ChordPro.h"

#include <algorithm>
#include <cctype>

namespace engine
{
namespace
{
std::string trim(const std::string& s)
{
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char) s[a]))
        ++a;
    while (b > a && std::isspace((unsigned char) s[b - 1]))
        --b;
    return s.substr(a, b - a);
}

std::string lower(std::string s)
{
    for (auto& c : s)
        c = (char) std::tolower((unsigned char) c);
    return s;
}

// Where a { or [ is closed, on the same line. npos if it is not.
size_t findClose(const std::string& text, size_t open, char close)
{
    for (size_t i = open + 1; i < text.size(); ++i)
    {
        if (text[i] == close)
            return i;
        if (text[i] == '\n' || text[i] == '\r')
            return std::string::npos;
    }
    return std::string::npos;
}

enum class Kind { None, Verse, Chorus, Bridge };

// start_of_verse, sov ... Gives the kind, and whether it starts (true) or ends (false) a section
bool sectionDirective(const std::string& name, Kind& kind, bool& starts)
{
    static const struct { const char* name; Kind kind; bool starts; } table[] = {
        { "start_of_verse", Kind::Verse, true },   { "sov", Kind::Verse, true },   { "end_of_verse", Kind::Verse, false },   { "eov", Kind::Verse, false },
        { "start_of_chorus", Kind::Chorus, true }, { "soc", Kind::Chorus, true }, { "end_of_chorus", Kind::Chorus, false }, { "eoc", Kind::Chorus, false },
        { "start_of_bridge", Kind::Bridge, true }, { "sob", Kind::Bridge, true }, { "end_of_bridge", Kind::Bridge, false }, { "eob", Kind::Bridge, false },
    };
    for (const auto& entry : table)
        if (name == entry.name)
        {
            kind = entry.kind;
            starts = entry.starts;
            return true;
        }
    return false;
}

const char* kindName(Kind kind)
{
    return kind == Kind::Verse ? "Verse" : kind == Kind::Chorus ? "Chorus" : "Bridge";
}

// G, F#m, Bb, Am, Dminor ... Gives false if it is not a key we can read
bool readKey(const std::string& text, ChordProResult& out)
{
    const auto word = trim(text);
    if (word.empty())
        return false;

    int pitch = -1;
    switch (std::toupper((unsigned char) word[0]))
    {
        case 'C': pitch = 0; break;
        case 'D': pitch = 2; break;
        case 'E': pitch = 4; break;
        case 'F': pitch = 5; break;
        case 'G': pitch = 7; break;
        case 'A': pitch = 9; break;
        case 'B': pitch = 11; break;
        default: return false;
    }
    size_t pos = 1;
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

    const auto rest = lower(word.substr(pos));
    if (rest == "" || rest == "maj" || rest == "major")
        out.keyMinor = false;
    else if (rest == "m" || rest == "min" || rest == "minor")
        out.keyMinor = true;
    else
        return false;

    out.keyTonic = wrap12(pitch);
    out.keyName = word;
    out.hasKey = true;
    return true;
}
}

ChordProResult readChordPro(const std::string& text)
{
    ChordProResult result;

    struct Open { Kind kind = Kind::None; std::string label; std::vector<std::string> chords; };
    Open open;
    std::vector<ChordProSection> sections;

    const auto close = [&]
    {
        if (open.kind != Kind::None && ! open.chords.empty())
        {
            ChordProSection section;
            section.name = open.label.empty() ? kindName(open.kind) : open.label;
            section.chords = open.chords;
            sections.push_back(section);
        }
        open = {};
    };

    size_t i = 0;
    const size_t length = text.size();
    bool lineStart = true;
    while (i < length)
    {
        const char c = text[i];
        if (c == '\n' || c == '\r')
        {
            lineStart = true;
            ++i;
            continue;
        }

        if (lineStart)
        {
            lineStart = false;
            size_t first = i;
            while (first < length && (text[first] == ' ' || text[first] == '\t'))
                ++first;
            if (first < length && text[first] == '#') // a comment: the rest of the line is not read
            {
                while (i < length && text[i] != '\n' && text[i] != '\r')
                    ++i;
                continue;
            }
        }

        if (c == '{')
        {
            const size_t end = findClose(text, i, '}');
            if (end == std::string::npos) // not closed on its line: just a character
            {
                ++i;
                continue;
            }
            const auto body = trim(text.substr(i + 1, end - i - 1));
            const size_t colon = body.find(':');
            size_t nameEnd = colon;
            if (nameEnd == std::string::npos)
                nameEnd = body.find_first_of(" \t");
            const auto name = lower(trim(body.substr(0, nameEnd == std::string::npos ? body.size() : nameEnd)));
            const auto arg = nameEnd == std::string::npos ? std::string() : trim(body.substr(nameEnd + 1));

            Kind kind;
            bool starts;
            if (name == "key")
                readKey(arg, result);
            else if (sectionDirective(name, kind, starts))
            {
                close();
                if (starts)
                {
                    open.kind = kind;
                    open.label = arg;
                }
            }
            i = end + 1;
            continue;
        }

        if (c == '[')
        {
            const size_t end = findClose(text, i, ']');
            if (end == std::string::npos)
            {
                ++i;
                continue;
            }
            const auto chord = trim(text.substr(i + 1, end - i - 1));
            // [*Coda] is a note in the song, not a chord
            if (! chord.empty() && chord[0] != '*')
            {
                result.chords.push_back(chord);
                if (open.kind != Kind::None)
                    open.chords.push_back(chord);
            }
            i = end + 1;
            continue;
        }

        ++i;
    }
    close();

    // The sections that came twice get a number: Chorus, Chorus 2
    std::vector<std::string> seen;
    for (auto& section : sections)
    {
        const int total = (int) std::count_if(sections.begin(), sections.end(), [&] (const ChordProSection& s) { return s.name == section.name; });
        if (total > 1)
        {
            const int before = (int) std::count(seen.begin(), seen.end(), section.name);
            seen.push_back(section.name);
            if (before > 0)
                section.name += " " + std::to_string(before + 1);
        }
    }
    result.sections = sections;
    return result;
}

bool isChordProFileName(const std::string& fileName)
{
    const auto dot = fileName.find_last_of('.');
    if (dot == std::string::npos)
        return false;
    const auto ending = lower(fileName.substr(dot + 1));
    return ending == "cho" || ending == "chopro" || ending == "crd" || ending == "pro";
}

std::string chordsAsQuickText(const std::vector<std::string>& chords, int maxChords, bool* tooMany)
{
    if (tooMany != nullptr)
        *tooMany = (int) chords.size() > maxChords;
    std::string text;
    for (size_t i = 0; i < chords.size() && (int) i < maxChords; ++i)
    {
        if (i > 0)
            text += ' ';
        text += chords[i];
    }
    return text;
}

bool chordProKeyDiffers(const ChordProResult& result, const Key& key)
{
    if (! result.hasKey)
        return false;
    const bool minorNow = key.scale != ScaleId::Major;
    return result.keyTonic != key.tonic || result.keyMinor != minorNow;
}
}
