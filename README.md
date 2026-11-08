# Progrezzor

Jam. Build. Drag. Done.

A very easy MIDI chord tool for every kind of music. Jam on the pads, find a progression, drag the MIDI out. You do not
need to know any music theory.

This is version 0.9.0, an internal test build that is never released (the first public release will be 1.0.0). See CHANGELOG.md. Free software (GNU AGPLv3). Built and tested on Linux, as
CLAP and VST3. Windows has not been tried yet.

## What it does

- **Seven big pads** with the chords of your key and scale (Minor, Major, Dorian, Phrygian). Each chord has its own colour and
  shows where it sits in the key (I, ii, III ...). Click them, or play the white keys of a MIDI keyboard. The gear on a pad gives
  other kinds of the same chord (7, maj7, sus, add9 and more) and inversions. A blank pad on the right is a pause (Rest).
- **Lock to scale.** On as standard, so the pads, Suggest and the chord menus stay in the key. When you want a chord from outside,
  the menus end with "More chords outside the key...", which switches the lock off and shows every chord.
- **Mood and Suggest.** Pick one of eight moods (Bright, Warm, Dreamy, Melancholic, Dark, Deep, Driving, Soulful) and press
  Suggest for a whole progression. Locked blocks stay. Suggest can use inversions for a smoother bass line (G/B), and you can
  switch that off in Settings.
- **The progression bar.** Drag a pad down to add a chord, or onto a block to replace it. Drag the right edge to change the
  length, drag a block to move it (hold Ctrl to copy it) and lock the ones you like. Right-click a block, or use its small gear,
  to change its chord type, inversion, octave and length, to lock, duplicate or delete it. Undo and redo work everywhere.
- **Rec.** Switch it on and every chord you play lands at the end of the bar as one block, from the mouse or from a MIDI keyboard.
- **Quick-text.** Type the chords, like `Am F C G:2`, and press Enter. `*` is how many, `:` is how long, `R` is a pause.
- **Learn.** Hold the right keys on a MIDI keyboard, chord after chord. The next chord is marked, and a keyboard shows the keys if
  you need a hint.
- **Arp and Rhythm.** The arp panel has a Hold | Arp | Rhythm switch. Arp plays the chord one note at a time (up, down, up and
  down, random) with a speed and a rhythm ring. Rhythm strikes the whole chord in one of 12 patterns (Offbeat, Stab, Double stab,
  Skank, Eighths, Push, Gallop, Charleston, Tresillo, Clave, Pulse, Euclid) at 1/8 or 1/16, short, medium or long. Both start
  again in every block and follow the DAW's timeline, and the MIDI file has the same notes.
- **Chord wheel.** The small wheel beside the key opens the circle of fifths. The chords of the seven pads have the pad colours;
  click a chord to hear it, double-click to use it as the key.
- **Bass note.** Settings has "Bass note": one low note under every chord. With the arp or rhythm on, it lies as one long note
  under it.
- **Preview sound.** A few small built-in sounds (Soft Pad, Strings, Keys, Organ, Stab, Pluck) let you hear what you play with no
  other synth. Choose **Off** when another synth plays the MIDI, or switch "Preview sound" off in Settings.
- **MIDI export.** Drag the MIDI tile into your DAW, or use Save MIDI and Open Folder. The file has the notes you hear: the chords,
  inversions, octaves, lengths, pauses, arp or rhythm and the bass note.
- **Play.** The progression follows the DAW when it runs (also over loops and jumps in time), and plays on its own when the DAW is
  stopped. Chord changes land exactly on the bar line. Notes never hang.

## Formats

- **CLAP** is a MIDI effect (note effect): it sits before a synth and sends it notes.
- **VST3** is an instrument: it makes MIDI (and its small preview sound) and sends the MIDI on to a synth.

Both are installed from `packaging/INSTALL-linux.txt`, or by a build (see below).

## Getting the notes to your synth

Progrezzor makes MIDI. A synth has to play it. If the synth plays the sound, set Progrezzor's **Preview sound to Off**
(the sound chooser in the top row), or you will hear both.

- **Bitwig Studio:** use the CLAP version. Put Progrezzor on an instrument track *before* the synth in the device chain, so the
  notes go through Progrezzor into the synth.
- **REAPER:** put Progrezzor first in the track's FX chain and the synth after it. Progrezzor's MIDI goes on to the synth.
- **Ableton Live:** (not tried yet on our side) put Progrezzor (VST3) on its own MIDI track, and on the synth's MIDI track set
  *MIDI From* to the Progrezzor track.

You can also drag the MIDI file from the Drag tile onto any MIDI track and play it from there.

## Build (Linux)

    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build --target Progrezzor_VST3 Progrezzor_CLAP clap_test_host
    ctest --test-dir build --output-on-failure

Needs JUCE 8.0.15 and clap-juce-extensions in ~/SDKs. A build puts the plug-ins in
`~/.clap` and `~/.vst3`.

## Notes for development

See CLAUDE.md (the plan and the rules), BACKLOG.md (ideas for later) and TESTLISTE.md (what to test).

The chord library is written from scratch for this project. Third-party licences (fonts and the plug-in libraries) are listed in
THIRD-PARTY-NOTICES.txt.
