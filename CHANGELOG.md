# Changelog

All notable changes to Progrezzor are listed here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and version numbers
follow [Semantic Versioning](https://semver.org/).

When releasing: move the entries under "Unreleased" into a new version
section, raise `project(Progrezzor VERSION …)` in `CMakeLists.txt` to match and
tag the commit (`vX.Y.Z`).

## [Unreleased]


### Fixed
- With Arp on and the loop running, a note from the old chord could sound right before the bar line, just before the new chord's first note. The arp steps now follow the host's grid.

### Added
- A menu on every block (right-click, or the small gear on mouseover): chord type, inversion, octave, length, Lock, Duplicate and Delete. Right-click no longer deletes at once; the Delete key still deletes the selected block.
- A bass note: Settings has "Bass note" (off as standard, saved with the project). It adds one low note under every chord, also in the loop and the MIDI file. With Arp on it lies as one long note under the arp.
- Suggest can now use inversions where they give a smoother bass line (C, G/B, Am, F). The block shows the name with a slash. Settings has "Inversions in suggestions" (on as standard); off gives only plain chords.
- **Learn.** A Learn button in the bar: hold the keys of each chord of the progression on a MIDI keyboard, one chord after the other, in any octave. The next chord is marked in the bar, a right chord ticks it off, and a wrong one flashes red. After two wrong tries, or from the start if you switch on hints in Settings, a piece of a keyboard shows which keys to hold.

### Changed
- A new project starts in C Major with the Bright mood (it was A Minor and Melancholic).
- The question "Replace the chords in the bar?" now sits in a small card in the middle of the bar, and Capture asks it too before it replaces chords you made.

## [0.1.0] – 2026-11-07

The first release. Linux, as CLAP and VST3. Other systems are not part of it yet.

### Added
- **Seven big pads** with the chords of the key and scale you pick (Minor, Major,
  Dorian, Phrygian). Every chord has its own colour, so you know it in any key.
  Each pad also shows where the chord sits in the key (I, ii, III …).
- **Play it your way.** Click the pads, or play the white keys of a MIDI keyboard.
  Six small built-in sounds (Pad, Strings, Keys, Organ, Stab, Pluck) let you hear
  what you play, or switch them off with Off.
- **The progression bar.** Drag a pad down to add a chord, or onto a block to
  replace it. Pull the right edge of a block to change its length (half a bar to
  eight bars), drag a block to move it, hold Ctrl to copy it, right-click to delete
  it, lock it with the padlock, move a chord up or down an octave, and undo and
  redo everything. **Capture** puts the last eight chords you played in the bar.
- **Suggest.** One click gives a whole new progression in the key and mood you
  chose. Eight moods: Bright, Warm, Dreamy, Melancholic, Dark, Deep, Driving and
  Soulful. Locked blocks stay, and Suggest asks before it replaces chords you made.
- **Play.** The progression follows the DAW when it plays, and plays on its own
  when the DAW is stopped. Chord changes land exactly on the bar line.
- **Quick-text.** Type the chords, like `Am F C G:2`, and press Enter. `*` is how
  many, `:` is how long, `+1` and `-1` move a chord an octave.
- **Lock to scale.** On by default. A chord outside the scale is marked and silent
  until you press Allow.
- **MIDI export.** Drag the MIDI tile into your DAW, or use Save MIDI and Open
  Folder. The file has exactly the notes you hear.
- **Arp.** Switch it on and everything is played one note at a time, with the MIDI
  file to match. Pick the pattern (up, down, up and down, random), the speed and the
  rhythm, shown as a ring.
- **Octave.** Oct- and Oct+ move all the chords up or down. A small keyboard
  shows where they sit (Mid C is marked) and lights the notes that sound. Settings
  lets you choose where the chords usually sit, for every project.
- **No hanging notes.** A note always ends with the one that started it, also when
  the key changes, the DAW stops, or the plug-in is bypassed.

### Known limits
- Tested on Linux only. Windows and its DAWs have not been tried yet.
- A chord always has the same notes. There is no automatic voice leading.
