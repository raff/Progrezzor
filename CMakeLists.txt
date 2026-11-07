# Copyright (C) 2026 Henrik Juhl. Part of Progrezzor, licensed under the GNU AGPLv3 (see LICENSE).
# SPDX-License-Identifier: AGPL-3.0-only

cmake_minimum_required(VERSION 3.22)
project(Progrezzor VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Same SDK versions as SeqVerse and SubForge. Override with -DJUCE_DIR=... / -DCLAP_JUCE_EXTENSIONS_DIR=...
#   JUCE                   8.0.15
#   clap-juce-extensions   7adee3a (0.26.0-117), with submodules
set(JUCE_DIR "$ENV{HOME}/SDKs/JUCE-8.0.15" CACHE PATH "Path to a JUCE source tree")
set(CLAP_JUCE_EXTENSIONS_DIR "$ENV{HOME}/SDKs/clap-juce-extensions-main" CACHE PATH
    "Path to a clap-juce-extensions source tree")

foreach(sdk_dir JUCE_DIR CLAP_JUCE_EXTENSIONS_DIR)
    if(NOT EXISTS "${${sdk_dir}}/CMakeLists.txt")
        message(FATAL_ERROR "${sdk_dir} is '${${sdk_dir}}' but no CMakeLists.txt was found there")
    endif()
endforeach()

add_subdirectory(${JUCE_DIR} juce EXCLUDE_FROM_ALL)
add_subdirectory(${CLAP_JUCE_EXTENSIONS_DIR} clap-juce-extensions EXCLUDE_FROM_ALL)

# The chord engine: pure C++, no JUCE, with its own unit tests.
add_library(progrezzor_engine STATIC
    src/engine/ChordEngine.cpp src/engine/ChordEngine.h src/engine/Music.h src/engine/NoteTracker.h
    src/engine/Library.cpp src/engine/Library.h src/engine/Progression.h
    src/engine/QuickText.cpp src/engine/QuickText.h src/engine/LoopPlan.cpp src/engine/LoopPlan.h src/engine/TripleBuffer.h src/engine/NextPads.h)
target_include_directories(progrezzor_engine PUBLIC src)
target_compile_options(progrezzor_engine PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
set_target_properties(progrezzor_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)

# The editor's fonts, as BinaryData (SIL Open Font License, see resources/fonts/*/OFL.txt)
juce_add_binary_data(ProgrezzorFonts SOURCES
    resources/fonts/ChakraPetch/ChakraPetch-SemiBold.ttf
    resources/fonts/ChakraPetch/ChakraPetch-Bold.ttf)
set_target_properties(ProgrezzorFonts PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Test branch test/vst3-instrument: the VST3 registers as an instrument (IS_SYNTH TRUE), the CLAP stays a note effect (its
# type comes from CLAP_FEATURES below, not from IS_SYNTH).
juce_add_plugin(Progrezzor
    COMPANY_NAME "Henrik"
    PLUGIN_MANUFACTURER_CODE Hjuh
    PLUGIN_CODE Prgz
    FORMATS VST3
    COPY_PLUGIN_AFTER_BUILD TRUE
    PRODUCT_NAME "Progrezzor"
    IS_SYNTH TRUE
    IS_MIDI_EFFECT FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE)

target_sources(Progrezzor PRIVATE
    src/PluginProcessor.cpp
    src/PluginProcessor.h
    src/PluginEditor.cpp
    src/PluginEditor.h
    src/PadSynth.h
    src/ProgrezzorLookAndFeel.h)

target_compile_definitions(Progrezzor PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Progrezzor
    PRIVATE
        ProgrezzorFonts
        progrezzor_engine
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

clap_juce_extensions_plugin(TARGET Progrezzor
    CLAP_ID "com.henrik.progrezzor"
    CLAP_FEATURES note-effect utility
    CLAP_USE_JUCE_PARAMETER_RANGES ALL)

# On Linux, every build puts the finished plugin in the user's own plugin folders (~/.vst3 and ~/.clap)
if(UNIX AND NOT APPLE)
    add_custom_command(TARGET Progrezzor_CLAP POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$ENV{HOME}/.clap"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:Progrezzor_CLAP>" "$ENV{HOME}/.clap/Progrezzor.clap"
        VERBATIM)
endif()

# Headless CLAP host for scripted tests. Loads the built .clap straight from the build tree.
#   cmake --build build --target Progrezzor_CLAP clap_test_host && ctest --test-dir build --output-on-failure
enable_testing()

add_executable(clap_test_host tests/clap_test_host.cpp)
target_include_directories(clap_test_host SYSTEM PRIVATE "${CLAP_JUCE_EXTENSIONS_DIR}/clap-libs/clap/include")
target_link_libraries(clap_test_host PRIVATE ${CMAKE_DL_LIBS} progrezzor_engine)
target_compile_options(clap_test_host PRIVATE -Wall -Wextra -Wpedantic)

add_test(NAME clap_host COMMAND clap_test_host $<TARGET_FILE:Progrezzor_CLAP>)

# Unit tests of the engine:
#   cmake --build build --target engine_test && ctest --test-dir build --output-on-failure
add_executable(engine_test tests/engine_test.cpp)
target_link_libraries(engine_test PRIVATE progrezzor_engine)
target_compile_options(engine_test PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
add_test(NAME engine_unit_tests COMMAND engine_test)

# Headless editor test: builds the real editor and processor without a host or a window, and plays pads. Given a file
# name prefix it also writes PNG snapshots of the editor.
#   cmake --build build --target ProgrezzorUiTest && ./build/ProgrezzorUiTest /tmp/editor
juce_add_console_app(ProgrezzorUiTest PRODUCT_NAME "ProgrezzorUiTest")

target_sources(ProgrezzorUiTest PRIVATE
    tests/ui_test.cpp
    src/PluginProcessor.cpp
    src/PluginEditor.cpp)

target_compile_definitions(ProgrezzorUiTest PRIVATE
    JucePlugin_Name="Progrezzor"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1) # the test runs the message loop for a while, so that the editor's timer fires

target_link_libraries(ProgrezzorUiTest
    PRIVATE
        ProgrezzorFonts
        progrezzor_engine
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

add_test(NAME ui_pads COMMAND ProgrezzorUiTest)
