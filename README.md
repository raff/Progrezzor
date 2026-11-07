# Progrezzor

Jam. Capture. Drag. Done.

A very easy MIDI chord tool: jam on the pads, find a progression, drag the MIDI out. No music theory needed.

Version 0.1.0. Free software (GNU AGPLv3). Linux, as CLAP and VST3. See CHANGELOG.md.

## Use

Put it on a MIDI track with a synth after it. Click the pads or play a MIDI keyboard, build a progression in the bar
at the bottom (or press Suggest), and drag the MIDI file into your DAW. Press Play to hear the progression.
See `packaging/INSTALL-linux.txt` for installing.

## Build (Linux)

    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build --target Progrezzor_VST3 Progrezzor_CLAP clap_test_host
    ctest --test-dir build --output-on-failure

Needs JUCE 8.0.15 and clap-juce-extensions in ~/SDKs. A build puts the plug-ins in
`~/.clap` and `~/.vst3`.

## Notes for development

See CLAUDE.md (the plan and the rules), BACKLOG.md (ideas for later) and TESTLISTE.md (what to test).
