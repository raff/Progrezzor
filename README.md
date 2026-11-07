# Progrezzor

Jam. Capture. Drag. Done.

Status: milestone 0 (empty plug-in, routing test). See CLAUDE.md.

## Build (Linux)

    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build --target Progrezzor_VST3 Progrezzor_CLAP clap_test_host
    ctest --test-dir build --output-on-failure

Needs JUCE 8.0.15 and clap-juce-extensions in ~/SDKs (same as SeqVerse and SubForge).
