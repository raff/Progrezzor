# Changelog

All notable changes to Progrezzor are listed here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and version numbers
follow [Semantic Versioning](https://semver.org/).

When releasing: move the entries under "Unreleased" into a new version
section, raise `project(Progrezzor VERSION …)` in `CMakeLists.txt` to match and
tag the commit (`vX.Y.Z`).

0.9.x are internal test builds and are never released. Everything since 0.1.0 is listed under "Unreleased" until 1.0.0.

## [Unreleased]

### Fixed
- **Piano, Keys, Stab and Pluck: a chord change that shares tones sounds as the whole chord.** D to F#m shares F# and A, and those were left ringing while they had almost faded, so only the new tone was heard. These sounds now strike the shared tones again (smoothly, no click). Soft Pad, Strings and Organ still carry shared tones on.
- **Editing while it plays no longer makes the loop jump.** The loop used to work out its place from the DAW's position modulo the length of the progression, so every change of a length moved the blocks under it. Now the loop keeps track of which block plays and how far it has got (blocks have a lasting identity), and counts on with the DAW's time. A change is heard the next time the loop reaches the changed place. A deleted block plays to its end, then the block after it goes on; a block made shorter than what is played goes on at the next half bar line; Clear, Suggest and Quick-text start from block 1 at the next block line. A stop and start, a jump or a loop in the DAW finds the place from the DAW's position as before.
- **Keys, Stab and Pluck: no jump when a note rings and is struck again.** The fading bell used to jump back to full strength on the very same sample (the same chord twice, or a tone two chords share). Now the same chord again goes quickly down to nothing and is struck anew, and a shared tone just carries on.
- **Soft Pad dip and click.** In a chord change (C to Em) the tones the two chords share dipped and swelled again, with a click. They now just carry on. Only when the very same chord is struck again does the level dip (smoothly, no click), so the second hit is still heard.

### Changed
- **Rec is now called Add.** The red record dot is replaced by a plus. It works as before: switched on, every chord you play is added to the end of the progression. Tooltip: "Add every chord you play to the end of the progression."
- **The Play button is never locked.** With the DAW stopped, Play starts and stops the progression on its own clock, from the marked block if one is marked, otherwise from block 1; stopping lets go of every note. With the DAW running, Play shows a small link icon and switches the progression on or off (it plays with the DAW as standard; off, it is silent but pads, MIDI keys and trying a chord on a block still work). The choice is saved with the project. If you dragged the MIDI into your DAW, mute the progression with Play, so it does not play twice.
- **Author name.** The plug-in now says "Henrik Juhl" as its author in the host (it said "Henrik"), like SubForge.
- **The DAW's play always starts the progression.** Before, the loop was silent while the DAW played if Play in the plug-in was off. Play now lights by itself while the DAW plays (and cannot be switched off), and goes out when the DAW stops.

### Added
- **Piano preview sound.** A real concert grand piano (Steinway B), from the CC0 Versilian Community Sample Library (see THIRD-PARTY-NOTICES.txt). It is second in the sound chooser, after Soft Pad. 21 small samples (about 1.5 MB) are built in; the notes between them are the nearest sample, stretched. The MIDI is the same as with the other sounds.
- **Edit while it plays.** Play only plays, it does not lock. Drag, length, the block menu, delete, Quick-text, Clear, Suggest and Rec work while the progression plays, also when the DAW runs. A change takes effect the next time that block begins, never in the middle of a chord.
- **Click a block while it plays:** it is marked (pulsing edge) and gives no sound on top of the loop. With Play off it is marked and plays its chord as before.
- **Try a chord on its place.** With a block marked, press a pad (mouse or MIDI): the block plays that chord for now (dashed edge), also in the loop. Enter or a new click on the block keeps it (one step for Undo). Esc, a click beside the blocks or a click on another block puts the old chord back.
- **Voicing: Nearest inversion.** Settings has a Voicing choice, "Root position" (as before) or "Nearest inversion". With Nearest inversion every chord is played in the way that lies nearest the home chord, so the notes hardly jump between chords (G7 in C Major is B3 D4 F4 G4). The name stays G7. A chord you gave a turn yourself is played as you chose. The pads, the loop and the MIDI file have the same notes. Saved with the project.
- **Copy as text and Save ChordPro.** Settings has "Copy as text" (the progression as a chord chart in bar notation, like `| C | Am | F G | G |`, which Quick-text reads back) and "Save ChordPro..." (a .cho file with the title, the key and the chords as a grid, so the lengths are kept). ChordPro import now reads grids too, so an exported file gives the same progression when it is imported again.
- **Keyboard: Chord roots.** Settings has a Keyboard choice, "White keys" (as before) or "Chord roots". With Chord roots a MIDI key plays the pad whose chord is built on that note, in every octave and also on black keys (Bb in D Minor); keys with no chord on them play nothing. Saved with the project.
- **Guess the key.** When you put chords in with Quick-text or import a ChordPro file, Progrezzor looks at which major or minor key they fit. If it is another key than the one you are in, a card asks "These chords fit A Minor." with Switch and Keep. It never switches by itself, and a file that says its own key is not guessed.
- **Chord charts in Quick-text.** Type or paste a chart with bar lines, like `| C | Am | F | G |`. Every bar is one block of a bar; two chords in a bar share it (`| C G |`); `%` or `/` repeats the chord before. Line breaks and extra spaces are ignored, the old notation (`Am*2`, `Am:2`) still works, and an unknown word is marked in red while the rest is used.
- **ChordPro import.** Settings has "Import ChordPro..." (or drag a .cho, .chopro, .crd or .pro file onto the bar). The chords in square brackets become Quick-text, one bar each, to look at and change before you press Enter. A file with parts (verse, chorus, bridge) asks which part you want. If the song says its key, it asks whether to switch; it never switches by itself.
- **Chord rhythm.** A Hold | Arp | Rhythm switch at the top of the arp panel. Rhythm strikes the whole chord in one of 12 patterns (Offbeat, Stab, Double stab, Skank, Eighths, Push, Gallop, Charleston, Tresillo, Clave, Pulse, Euclid), at 1/8 or 1/16, with Short, Medium or Long notes. It starts again in every block, follows the host's grid, and the MIDI file has the same strikes.
- **Chord wheel.** A small wheel beside the key opens the circle of fifths in a panel that stays open until you close it. The chords of the seven pads have the pad colours, the others are dim. Click a chord to hear it, double-click to use it as the key.
- **Block menu.** Right-click a block, or use the small gear on mouseover, to change its chord type, inversion, octave and length, or to lock, duplicate or delete it. Right-click no longer deletes at once; the Delete key still deletes the selected block. A pause has only length, Duplicate and Delete.
- **More chords outside the key.** With Lock to scale on (as standard), the chord menus of the pads and the blocks end with "More chords outside the key...", which turns the lock off and opens the menu again with every chord.
- **Bass note.** Settings has "Bass note" (off as standard, saved with the project): one low note under every chord, also in the loop and the MIDI file. With the arp or rhythm on it lies as one long note under it.
- **Suggest with inversions.** Suggest can use inversions where they give a smoother bass line (C, G/B, Am, F). The block shows the name with a slash. Settings has "Inversions in suggestions" (on as standard).
- **Rec.** A switch with a red dot: while it is on, every chord you play (mouse or MIDI keyboard) is added to the end of the bar as one block. It replaces Capture.
- **Rests.** A blank pad on the right is a pause. Drag it into the bar, or click it with Rec on. A pause is silent in the loop and in the MIDI file, and Quick-text knows it as `R`.
- **Learn.** A Learn button in the bar: hold the keys of each chord of the progression on a MIDI keyboard, one chord after the other, in any octave. The next chord is marked in the bar, a right chord ticks it off, and a wrong one flashes red. After two wrong tries, or from the start if you switch on hints in Settings, a piece of a keyboard shows which keys to hold.
- **Preview sound.** The sound chooser has a loudspeaker (crossed out and dim when the sound is Off), a line over Off and a clearer tooltip. Settings has "Preview sound" to switch the sound off and on. The sound "Pad" is now called "Soft Pad".
- **Tooltips** are narrower (several lines), lie clear of the mouse and have a coloured edge. Settings has "Show tooltips" to switch them off.
- **Clearer text.** Small text uses the Inter typeface (SemiBold), which is easier to read on a screen; the big names (logo, chords, Suggest) keep the display face. Nothing is smaller than 13 px, and text keeps its distance from the edges of buttons, tiles and blocks.
- **VST3 as an instrument.** The VST3 registers as an instrument, so it can be put on an instrument track; the CLAP stays a MIDI effect (note effect).
- A new project starts in C Major with the Bright mood. The question "Replace the chords in the bar?" sits in a small card in the middle of the bar.

### Fixed
- **Windows display scaling in the CLAP version.** With Windows scaling above 100 % (125 %, 150 %) the window came out too big and was cropped in the host. The CLAP editor now stays at 1:1 like the VST3 one, the same fix as in SubForge and SpaceForge.
- The chords no longer jump an octave in the middle of the scale. The root of the first chord sits between F#3 and F4, every other chord has its root 0 to 11 semitones above it, so the seven pads always climb (C4 D4 E4 F4 G4 A4 B4 in C Major).
- With the arp on and the loop running, a note from the old chord could sound right before the bar line, just before the new chord's first note. The arp steps now follow the host's grid.
- Text sitting too close to the edge of buttons and tiles (Short, Medium, Long, long pattern names and more), and a Settings gear that was too small, are fixed.

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
