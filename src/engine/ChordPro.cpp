// Copyright (C) 2026 Henrik Juhl. Part of Progrezzor, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only
#include "ChordPro.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

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
// 0.5, 1.5, 2: no zeros at the end
std::string formatBarsText(double bars)
{
    char text[32];
    std::snprintf(text, sizeof(text), "%.2f", bars);
    std::string s = text;
    while (! s.empty() && s.back() == '0')
        s.pop_back();
    if (! s.empty() && s.back() == '.')
        s.pop_back();
    return s;
}

// The cells in a measure from shape="4x4", "1+4x4+1" or "4x4"; 0 if the shape does not say
int cellsFromShape(const std::string& argument)
{
    std::string shape = argument;
    const auto equals = shape.find('=');
    if (equals != std::string::npos)
        shape = shape.substr(equals + 1);
    shape.erase(std::remove_if(shape.begin(), shape.end(), [] (char c) { return c == '"' || c == '\'' || std::isspace((unsigned char) c); }), shape.end());

    // left+measures x beats+right: the part with the x
    size_t start = 0;
    while (start <= shape.size())
    {
        size_t plus = shape.find('+', start);
        const auto part = shape.substr(start, plus == std::string::npos ? std::string::npos : plus - start);
        const auto x = part.find_first_of("xX");
        if (x != std::string::npos)
            return std::atoi(part.substr(x + 1).c_str());
        if (plus == std::string::npos)
            break;
        start = plus + 1;
    }
    return 0;
}

struct GridMeasure
{
    std::vector<std::string> cells;
};

// The grid between {start_of_grid ...} and {end_of_grid}: the blocks as Quick-text words
void readGrid(const std::string& body, int shapeCells, std::vector<std::string>& words)
{
    std::vector<GridMeasure> measures; // what was read (for % and %%)
    bool haveBlock = false;
    std::string blockWord;
    double blockBars = 0.0;
    const auto finish = [&]
    {
        if (haveBlock)
            words.push_back(blockWord + (std::abs(blockBars - 1.0) > 0.005 ? ":" + formatBarsText(blockBars) : std::string()));
        haveBlock = false;
    };
    const auto addMeasure = [&] (const GridMeasure& measure)
    {
        const int n = shapeCells > 0 ? shapeCells : (int) measure.cells.size();
        if (n <= 0)
            return;
        for (const auto& cell : measure.cells)
        {
            if (cell == "." || cell == "/")
            {
                if (haveBlock)
                    blockBars += 1.0 / n;
                continue;
            }
            // Several chords in a cell (A~B): the first one
            auto chord = cell.substr(0, cell.find('~'));
            if (chord.empty())
                continue;
            finish();
            std::string upper = chord;
            for (auto& c : upper)
                c = (char) std::toupper((unsigned char) c);
            blockWord = (upper == "N.C." || upper == "NC") ? "R" : chord;
            blockBars = 1.0 / n;
            haveBlock = true;
        }
    };

    int skipNext = 0;
    GridMeasure forcedNext;
    bool haveForced = false;

    size_t lineStart = 0;
    while (lineStart <= body.size())
    {
        size_t lineEnd = body.find_first_of("\r\n", lineStart);
        if (lineEnd == std::string::npos)
            lineEnd = body.size();
        const auto line = body.substr(lineStart, lineEnd - lineStart);
        lineStart = lineEnd + 1;

        // Tokens: bar symbols (| || |. |: :| :|: |1 :|2>) and cells
        struct Token { bool bar; std::string text; };
        std::vector<Token> tokens;
        size_t i = 0;
        while (i < line.size())
        {
            const char c = line[i];
            if (std::isspace((unsigned char) c))
            {
                ++i;
                continue;
            }
            if (c == '|' || (c == ':' && i + 1 < line.size() && (line[i + 1] == '|' || line[i + 1] == ':')))
            {
                size_t j = i;
                while (j < line.size() && (line[j] == '|' || line[j] == ':' || line[j] == '.'))
                    ++j;
                while (j < line.size() && std::isdigit((unsigned char) line[j]))
                    ++j;
                if (j < line.size() && line[j] == '>')
                    ++j;
                tokens.push_back({ true, line.substr(i, j - i) });
                i = j;
            }
            else
            {
                size_t j = i;
                while (j < line.size() && ! std::isspace((unsigned char) line[j]) && line[j] != '|')
                    ++j;
                tokens.push_back({ false, line.substr(i, j - i) });
                i = j;
            }
        }

        // A line needs a bar symbol. A strum line has S or s right after the first one: not read.
        size_t firstBar = tokens.size();
        for (size_t t = 0; t < tokens.size(); ++t)
            if (tokens[t].bar)
            {
                firstBar = t;
                break;
            }
        if (firstBar == tokens.size())
            continue;
        {
            const auto barEnd = line.find(tokens[firstBar].text) + tokens[firstBar].text.size();
            if (barEnd < line.size() && (line[barEnd] == 's' || line[barEnd] == 'S') && (barEnd + 1 >= line.size() || std::isspace((unsigned char) line[barEnd + 1])))
                continue;
        }

        // The measures: between one bar symbol and the next. What is before the first and after the last is margin.
        size_t t = firstBar;
        while (t < tokens.size())
        {
            size_t next = t + 1;
            GridMeasure measure;
            while (next < tokens.size() && ! tokens[next].bar)
                measure.cells.push_back(tokens[next++].text);
            const bool closed = next < tokens.size(); // a bar symbol ends it
            t = next;
            if (! closed)
                break;
            if (measure.cells.empty())
                continue;

            if (skipNext > 0) // the second measure of a %% is the measure it repeats
            {
                --skipNext;
                if (haveForced)
                    addMeasure(forcedNext);
                haveForced = false;
                measures.push_back(forcedNext);
                continue;
            }
            const auto& first = measure.cells.front();
            if (first == "%" && ! measures.empty())
            {
                const auto again = measures.back();
                addMeasure(again);
                measures.push_back(again);
            }
            else if (first == "%%" && measures.size() >= 2)
            {
                const auto a = measures[measures.size() - 2], b = measures.back();
                addMeasure(a);
                measures.push_back(a);
                forcedNext = b;
                haveForced = true;
                skipNext = 1;
            }
            else if (first == "%" || first == "%%")
                continue;
            else
            {
                addMeasure(measure);
                measures.push_back(measure);
            }
        }
    }
    finish();
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
            if (name == "start_of_grid" || name == "sog")
            {
                // Everything up to {end_of_grid} is the grid
                size_t gridEnd = length;
                size_t after = length;
                for (size_t search = end + 1; search < length;)
                {
                    const size_t brace = text.find('{', search);
                    if (brace == std::string::npos)
                        break;
                    const size_t close = text.find('}', brace);
                    if (close == std::string::npos)
                        break;
                    const auto inner = trim(text.substr(brace + 1, close - brace - 1));
                    const auto innerName = lower(trim(inner.substr(0, inner.find_first_of(": \t"))));
                    if (innerName == "end_of_grid" || innerName == "eog")
                    {
                        gridEnd = brace;
                        after = close + 1;
                        break;
                    }
                    search = close + 1;
                }
                std::vector<std::string> words;
                readGrid(text.substr(end + 1, gridEnd - (end + 1)), cellsFromShape(arg), words);
                for (const auto& word : words)
                {
                    result.chords.push_back(word);
                    if (open.kind != Kind::None)
                        open.chords.push_back(word);
                }
                i = after;
                continue;
            }
            else if (name == "key")
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

std::string exportChordPro(const Progression& progression, const Key& key, double barBeats, const std::string& title)
{
    // How many cells a measure has: its beats, but an even number, so that half a bar is whole cells
    int cells = (int) std::lround(barBeats);
    if (cells < 2)
        cells = 2;
    if (cells % 2 != 0)
        cells *= 2;
    const int measuresPerLine = 4;

    std::string text = "{title: " + title + "}\n";
    text += "{key: " + keyName(key) + (key.scale == ScaleId::Major ? "" : "m") + "}\n";
    text += "{start_of_grid shape=\"" + std::to_string(measuresPerLine) + "x" + std::to_string(cells) + "\"}\n";

    // The cells in order: a chord, then a dot for each cell it goes on
    std::vector<std::string> stream;
    for (const auto& block : progression)
    {
        const int n = std::max(1, (int) std::lround(block.beats / barBeats * cells));
        stream.push_back(block.rest ? "N.C." : chordName(block.chord, key));
        for (int c = 1; c < n; ++c)
            stream.push_back(".");
    }

    size_t at = 0;
    while (at < stream.size())
    {
        std::string line = "|";
        for (int measure = 0; measure < measuresPerLine && at < stream.size(); ++measure)
        {
            for (int c = 0; c < cells && at < stream.size(); ++c)
                line += " " + stream[at++];
            line += " |";
        }
        text += line + "\n";
    }
    text += "{end_of_grid}\n";
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
