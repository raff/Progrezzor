# Progrezzor

**Jam. Build. Drag. Done.**

A very easy MIDI chord tool for every kind of music. Jam on the pads, find a progression you like, and drag it into your DAW as
MIDI. You do not need to know any music theory.

Version 1.0.0. Free and open source (GNU AGPLv3). CLAP and VST3 for Linux and Windows.

## Quick start

1. Put Progrezzor on a MIDI track with a synth after it (see [Getting the notes to your synth](#getting-the-notes-to-your-synth)).
2. Click the big pads, or play the white keys of a MIDI keyboard. Every pad is a chord that fits the key.
3. Press **Suggest** for a whole progression, or drag pads down into the progression bar to build your own.
4. Press **Play** (or start your DAW) to hear the progression loop.
5. Drag the **MIDI** tile into your DAW. Done.

## What it does

- **Seven big pads** with the chords of your key and scale (Major, Minor, Dorian, Phrygian). Each chord has its own colour and
  shows its place in the key (I, ii, iii ...). The gear on a pad gives other kinds of the same chord (7, maj7, sus, add9 and
  more) and inversions. The blank pad on the right is a pause (Rest).
- **Lock to scale.** On as standard, so pads, Suggest and the chord menus stay in the key. Need a chord from outside? The menus
  end with "More chords outside the key...".
- **Mood and Suggest.** Pick one of eight moods (Bright, Warm, Dreamy, Melancholic, Dark, Deep, Driving, Soulful) and press
  Suggest for a whole progression. Locked blocks stay where they are.
- **The progression bar.** Drag a pad down to add a chord, or onto a block to replace it. Drag a block's right edge to change
  its length, drag a block to move it (hold Ctrl to copy it). Right-click a block, or use its small gear, to change its chord,
  inversion, octave and length, or to lock, duplicate or delete it. Undo and redo work everywhere.
- **Edit while it plays.** Change the bar at any time. A change is heard when the loop next reaches that block, never in the
  middle of a chord. Click a block to mark it, then press a pad to try that chord in its place (dashed edge). Enter or a new
  click on the block keeps it, Esc puts the old chord back.
- **Add.** Switch it on, and every chord you play is added to the end of the progression, from the mouse or a MIDI keyboard.
- **Quick-text.** Type the chords and press Enter: `Am F C G:2`. `*` is how many (`Am*2`), `:` is how long in bars (`Am:2`),
  `R` is a pause. Chord charts with bars work too: `| C | Am | F G |`.
- **Import and export.** Import a ChordPro file (`.cho`) or a plain chord sheet from Settings. The chords are shown in
  Quick-text first, so you can check them before they go in. Progrezzor can guess the key and asks before it changes it.
  Export the progression as ChordPro, or copy it as text.
- **Learn.** Play the chords of the progression on a MIDI keyboard, one after the other. The next chord is marked, and a small
  keyboard shows the keys if you need a hint.
- **Arp and Rhythm.** Hold plays the chord as it is. Arp plays it one note at a time. Rhythm strikes the whole chord in one of
  12 patterns (Offbeat, Stab, Double stab, Skank, Eighths, Push, Gallop, Charleston, Tresillo, Clave, Pulse, Euclid). Both
  follow your DAW's timeline, and the MIDI file has the same notes.
- **Chord wheel.** The wheel beside the key opens the circle of fifths, with the pad chords in their colours. Click a chord to
  hear it.
- **Voicing and bass note.** In Settings, "Nearest inversion" keeps each chord close to the home chord, like a keyboard player
  keeping the hand in one place. "Bass note" adds one low root note under every chord.
- **Preview sound.** Built-in sounds (Soft Pad, Piano, Strings, Keys, Organ, Stab, Pluck) let you hear what you play with no
  other synth. Choose **Off** when another synth plays the MIDI.
- **MIDI export.** Drag the MIDI tile into your DAW, or use Save MIDI and Open Folder. The file has exactly the notes you hear.

## Good to know

- **Play.** When your DAW plays, the progression plays with it, always. When your DAW is stopped, Play starts and stops the
  progression on its own clock, from the first block.
- **Two blocks, two strikes.** Two blocks with the same chord strike it twice. For one long chord, make one longer block.
- **Keyboard.** The white keys play pad 1 to 7 (C to B) in every octave. The black keys do nothing, except in Learn.

## Getting the notes to your synth

Progrezzor makes MIDI, and a synth plays it. When your synth plays, set Progrezzor's sound to **Off** (the sound menu in the top
row), or you will hear both.

- **Bitwig Studio:** use the CLAP version. Put Progrezzor before the synth on the same track.
- **REAPER:** put Progrezzor first in the track's FX chain and the synth after it.
- **Ableton Live:** put Progrezzor (VST3) on its own MIDI track. On the synth's track, set *MIDI From* to the Progrezzor track
  and Monitor to *In*.
- **FL Studio:** FL only tells a plug-in that it is playing when the track has a clip in the playlist. Put any clip on it (an
  empty one is enough). Otherwise use Progrezzor's own Play button.

**After you drag the MIDI into your DAW,** bypass Progrezzor on that track. Otherwise the clip's notes play Progrezzor's pads,
and the progression plays twice.

## Install

Download the archive for your system from the
[Releases page](https://github.com/travisdk/Progrezzor/releases/latest) and follow `INSTALL-linux.txt` or
`INSTALL-windows.txt` inside it.

- **Linux:** copy `Progrezzor.clap` to `~/.clap/` and `Progrezzor.vst3` to `~/.vst3/`.
- **Windows:** copy them to `%LOCALAPPDATA%\Programs\Common\CLAP\` and `%LOCALAPPDATA%\Programs\Common\VST3\`.
- **macOS:** copy them to `~/Library/Audio/Plug-Ins/CLAP/` and `~/Library/Audio/Plug-Ins/VST3/` (see `INSTALL-macos.txt`).

Then restart your DAW or rescan plug-ins.

## Build (Linux)

    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build --target Progrezzor_VST3 Progrezzor_CLAP clap_test_host
    ctest --test-dir build --output-on-failure

Needs JUCE 8.0.15 and clap-juce-extensions in `~/SDKs`. A build puts the plug-ins in `~/.clap` and `~/.vst3`.

## Build (macOS)

You need Xcode's command line tools, CMake and Ninja:

    xcode-select --install
    brew install cmake ninja

Fetch the two SDKs into `~/SDKs` (the folder names matter):

    mkdir -p ~/SDKs && cd ~/SDKs
    git clone --depth 1 --branch 8.0.15 https://github.com/juce-framework/JUCE.git JUCE-8.0.15
    git clone https://github.com/free-audio/clap-juce-extensions.git clap-juce-extensions-main
    git -C clap-juce-extensions-main checkout 7adee3a1bd4684d4caa5601100e364abccff4b4d
    git -C clap-juce-extensions-main submodule update --init --recursive

Then, from the Progrezzor folder:

    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build --target Progrezzor_VST3 Progrezzor_CLAP engine_test clap_test_host ProgrezzorUiTest
    ctest --test-dir build --output-on-failure

The build is universal (Apple silicon and Intel, macOS 11 or newer). It copies the plug-ins to
`~/Library/Audio/Plug-Ins/CLAP` and `~/Library/Audio/Plug-Ins/VST3`. Add `-DPROGREZZOR_INSTALL_AFTER_BUILD=OFF` to
skip the copy, or `-DJUCE_DIR=...` / `-DCLAP_JUCE_EXTENSIONS_DIR=...` to use SDKs somewhere else. To make the release
archive, build the target `package-macos`.

The Mac build is not signed. If macOS refuses to load a plug-in, run `xattr -cr` on the `.clap` and `.vst3` once, then
restart your DAW or rescan plug-ins.

## Licence and credits

Progrezzor is free software under the GNU Affero General Public License v3 (see LICENSE). It comes with no warranty.

- Built with JUCE (AGPLv3) and clap-juce-extensions.
- The Piano sound uses samples from the Versilian Community Sample Library (CC0) by Versilian Studios.
- Fonts: Chakra Petch and Inter (SIL Open Font License).
- The chord library is written from scratch for this project.

All third-party licences are in THIRD-PARTY-NOTICES.txt. VST is a registered trademark of Steinberg Media Technologies GmbH.

For development notes, see CLAUDE.md, BACKLOG.md and TESTLISTE.md.
