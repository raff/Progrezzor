# Backlog

Ideas outside the current milestone go here.

- Test on Windows (incl. Ableton Live) once CI release builds exist. Ableton does not run on Linux, so milepæl 0's Ableton check and the "confirmed in Ableton" MIDI export check wait for that.
- Copy CI/release setup from SubForge (.github/workflows) when it is time for Windows builds.
- Preview sound on/off (or volume), so it can be muted when the DAW's own synth plays the MIDI. Today the Pad sound always plays.
- Pad click velocity from where on the pad you click (now fixed at 100).
- Chord names are spelled with only flats or only sharps (Cb is written B). Proper spelling per key if it ever matters.
- Suggest with locked blocks: locked blocks keep their place, the rest comes from a fresh suggestion. Later: prefer library progressions that fit the locked chords.
- Lengths are in steps of 2 beats, so a half bar in 3/4 (1.5 beats) cannot be dragged to; Suggest and "+" still use the host's bar.
- MIDI export: only the newest export is remembered, so dragging an older progression again writes a new numbered copy. Files are never deleted.
- Lit pads (suggested next chords): paused after milestone 7 test, the hint was not clear. The table is in src/engine/NextPads.h and the glow in PadButton::setSuggested; switch it on with kShowSuggestedPads in PluginEditor.cpp. Needs a clearer look (a small mark instead of a glowing edge) and a tooltip.

- Quick-text: a block that is disabled (typed outside the scale, lock on) is only marked by the text mode when Enter is pressed; the red message and Allow show under the blocks. If Allow is wanted per chord instead of for all, that is a later change.
- Quick-text: blocks are matched to the old ones by place, so a chord moved by typing counts as a new block (it loses its 🔒). Matching by order of chords would keep it.
- Quick-text: a half bar in 3/4 is 1.5 beats, but the smallest length is 2 beats, so "Am:0.5" becomes 2 beats there (same as the dragged lengths above).
- pluginval is in ~/SDKs/pluginval (it was missed before). Run on the branch test/vst3-instrument at strictness 10 without GUI tests: SUCCESS. The GUI tests are skipped (no display here).

- Octave is on the blocks in the bar only (Henrik's choice: not on the pads/preview). Suggest and Capture give blocks in the normal register. Quick-text has no way to write an octave: a block keeps its octave only while its chord is unchanged at the same place.

## Senere

- Eget tempo-felt til Play, når DAW'en står stille (nu bruges DAW'ens tempo, ellers 120 BPM).
- Flere skalaer (nu kun Minor, Major, Dorian, Phrygian). SeqVerse har omkring 40, men Progrezzor skal kunne bygge 7 pads med pæne treklange, så kun skalaer med 7 toner kommer i betragtning. Forslag i prioriteret rækkefølge: Mixolydian, Lydian (let), Harmonic Minor, Melodic Minor, senere Locrian, Phrygian Dominant, Harmonic Major. Hver ny skala skal have: pad-akkorder, stavning af toner (b eller #), og nok rundgange i alle 8 stemninger (mindst 5 der kan foreslås, ellers bruger Suggest kun pads). Pentatoniske skalaer, blues og hel-tone passer ikke til de 7 pads.
- Arp: BYGGET 2026-11-07 (se CLAUDE.md, afsnittet Arp). Ikke med endnu: chance for huller i rytmen, flere retninger fra SeqVerse (Converge, Thumb, Pinky, osv.), swing, notelængde, og et lys i ringen der viser hvilket trin der spiller nu.
