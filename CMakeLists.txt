# Copyright (C) 2026 Henrik Juhl. Part of Progrezzor, licensed under the GNU AGPLv3 (see LICENSE).
# SPDX-License-Identifier: AGPL-3.0-only

cmake_minimum_required(VERSION 3.22)
project(Progrezzor VERSION 0.9.10 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC: link the C++ runtime statically (/MT). A plug-in runs inside the host's process, so a dynamic MSVCP140.dll would have to
# be installed on the user's PC and could be an older version already loaded by the host. Set before any target is created, so
# JUCE and the CLAP wrapper match. (Same as SubForge.)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Same SDK versions as SeqVerse and SubForge. Override with -DJUCE_DIR=... / -DCLAP_JUCE_EXTENSIONS_DIR=...
#   JUCE                   8.0.15
#   clap-juce-extensions   7adee3a (0.26.0-117), with submodules
# SDKs default to ~/SDKs (Linux) or %USERPROFILE%\SDKs (Windows).
if(WIN32)
    file(TO_CMAKE_PATH "$ENV{USERPROFILE}" PROGREZZOR_HOME)
else()
    set(PROGREZZOR_HOME "$ENV{HOME}")
endif()
set(JUCE_DIR "${PROGREZZOR_HOME}/SDKs/JUCE-8.0.15" CACHE PATH "Path to a JUCE source tree")
set(CLAP_JUCE_EXTENSIONS_DIR "${PROGREZZOR_HOME}/SDKs/clap-juce-extensions-main" CACHE PATH
    "Path to a clap-juce-extensions source tree")

# Per-user plug-in folders from the CLAP and VST3 specifications (no administrator rights needed)
if(WIN32)
    file(TO_CMAKE_PATH "$ENV{LOCALAPPDATA}" PROGREZZOR_LOCALAPPDATA)
    set(PROGREZZOR_DEFAULT_CLAP_DIR "${PROGREZZOR_LOCALAPPDATA}/Programs/Common/CLAP")
    set(PROGREZZOR_DEFAULT_VST3_DIR "${PROGREZZOR_LOCALAPPDATA}/Programs/Common/VST3")
else()
    set(PROGREZZOR_DEFAULT_CLAP_DIR "${PROGREZZOR_HOME}/.clap")
    set(PROGREZZOR_DEFAULT_VST3_DIR "${PROGREZZOR_HOME}/.vst3")
endif()
option(PROGREZZOR_INSTALL_AFTER_BUILD "Copy the CLAP and VST3 into the user's plug-in folders after each build" ON)
set(CLAP_INSTALL_DIR "${PROGREZZOR_DEFAULT_CLAP_DIR}" CACHE PATH "Where the built .clap is copied")
set(VST3_INSTALL_DIR "${PROGREZZOR_DEFAULT_VST3_DIR}" CACHE PATH "Where the built .vst3 is copied")

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
    src/engine/QuickText.cpp src/engine/QuickText.h src/engine/ChordPro.cpp src/engine/ChordPro.h src/engine/KeyGuess.cpp src/engine/KeyGuess.h src/engine/LoopPlan.cpp src/engine/LoopPlan.h src/engine/TripleBuffer.h src/engine/NextPads.h)
target_include_directories(progrezzor_engine PUBLIC src)
if(NOT MSVC)
    target_compile_options(progrezzor_engine PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()
set_target_properties(progrezzor_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)

# The editor's fonts, as BinaryData (SIL Open Font License, see resources/fonts/*/OFL.txt)
juce_add_binary_data(ProgrezzorFonts SOURCES
    resources/fonts/ChakraPetch/ChakraPetch-SemiBold.ttf
    resources/fonts/ChakraPetch/ChakraPetch-Bold.ttf
    resources/fonts/Inter/Inter-SemiBold.otf)
set_target_properties(ProgrezzorFonts PROPERTIES POSITION_INDEPENDENT_CODE ON)

# The samples of the Piano preview sound, as BinaryData of their own (FLAC, mono, 44.1 kHz, CC0 from the Versilian Community Sample
# Library, see THIRD-PARTY-NOTICES.txt). tools/make_piano_samples.py makes them.
file(GLOB PROGREZZOR_PIANO_SAMPLES "${CMAKE_CURRENT_SOURCE_DIR}/resources/piano/piano_*.flac")
juce_add_binary_data(ProgrezzorPiano HEADER_NAME PianoData.h NAMESPACE PianoData SOURCES ${PROGREZZOR_PIANO_SAMPLES})
set_target_properties(ProgrezzorPiano PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Test branch test/vst3-instrument: the VST3 registers as an instrument (IS_SYNTH TRUE), the CLAP stays a note effect (its
# type comes from CLAP_FEATURES below, not from IS_SYNTH).
juce_add_plugin(Progrezzor
    COMPANY_NAME "Henrik Juhl"
    PLUGIN_MANUFACTURER_CODE Hjuh
    PLUGIN_CODE Prgz
    FORMATS VST3
    COPY_PLUGIN_AFTER_BUILD ${PROGREZZOR_INSTALL_AFTER_BUILD}   # VST3 (the CLAP is copied by the command further down)
    VST3_COPY_DIR "${VST3_INSTALL_DIR}"
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
    JUCE_USE_OGGVORBIS=0 # only FLAC is read (the Piano samples): the other audio formats are left out, to keep the plug-in small
    JUCE_USE_MP3AUDIOFORMAT=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Progrezzor
    PRIVATE
        ProgrezzorFonts
        ProgrezzorPiano
        progrezzor_engine
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

clap_juce_extensions_plugin(TARGET Progrezzor
    CLAP_ID "com.henrik.progrezzor"
    CLAP_FEATURES note-effect utility
    CLAP_USE_JUCE_PARAMETER_RANGES ALL)

# Every build puts the finished plugin in the user's own plugin folders (unless PROGREZZOR_INSTALL_AFTER_BUILD is OFF, as on CI)
if(PROGREZZOR_INSTALL_AFTER_BUILD)
    add_custom_command(TARGET Progrezzor_CLAP POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CLAP_INSTALL_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:Progrezzor_CLAP>" "${CLAP_INSTALL_DIR}/"
        COMMENT "Installing Progrezzor.clap into ${CLAP_INSTALL_DIR}"
        VERBATIM)
endif()

# Headless CLAP host for scripted tests. Loads the built .clap straight from the build tree.
#   cmake --build build --target Progrezzor_CLAP clap_test_host && ctest --test-dir build --output-on-failure
enable_testing()

if(NOT WIN32) # the headless host uses dlopen
    add_executable(clap_test_host tests/clap_test_host.cpp)
    target_include_directories(clap_test_host SYSTEM PRIVATE "${CLAP_JUCE_EXTENSIONS_DIR}/clap-libs/clap/include")
    target_link_libraries(clap_test_host PRIVATE ${CMAKE_DL_LIBS} progrezzor_engine)
    target_compile_options(clap_test_host PRIVATE -Wall -Wextra -Wpedantic)

    add_test(NAME clap_host COMMAND clap_test_host $<TARGET_FILE:Progrezzor_CLAP>)
endif()

# Unit tests of the engine:
#   cmake --build build --target engine_test && ctest --test-dir build --output-on-failure
add_executable(engine_test tests/engine_test.cpp)
target_link_libraries(engine_test PRIVATE progrezzor_engine)
if(NOT MSVC)
    target_compile_options(engine_test PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()
target_compile_definitions(engine_test PRIVATE CHORDPRO_TEST_DIR="${CMAKE_SOURCE_DIR}/tests/chordpro") # the small example files
add_test(NAME engine_unit_tests COMMAND engine_test)

if(NOT WIN32) # the editor test sets environment variables the POSIX way
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
        JUCE_USE_OGGVORBIS=0
        JUCE_USE_MP3AUDIOFORMAT=0
        CHORDPRO_TEST_DIR="${CMAKE_SOURCE_DIR}/tests/chordpro" # the small example files
        JUCE_MODAL_LOOPS_PERMITTED=1) # the test runs the message loop for a while, so that the editor's timer fires

    target_link_libraries(ProgrezzorUiTest
        PRIVATE
            ProgrezzorFonts
            ProgrezzorPiano
            progrezzor_engine
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

    add_test(NAME ui_pads COMMAND ProgrezzorUiTest)
endif()

# Archive with the CLAP and VST3 plug-ins, the licence, the changelog, the third-party notices and install notes:
# dist/Progrezzor-linux-x86_64.tar.gz on Linux, dist/Progrezzor-windows-x64.zip on Windows. The file names carry no version; the
# folder inside the archive does. (Same set-up as SubForge.) These are internal test builds (0.9.x) and are not published.
if(WIN32)
    set(PROGREZZOR_PACKAGE_TARGET package-windows)
    set(PROGREZZOR_PACKAGE_NAME "Progrezzor-${PROJECT_VERSION}-windows-x64")
    set(PROGREZZOR_PACKAGE_FILE "Progrezzor-windows-x64.zip")
    set(PROGREZZOR_TAR_FLAGS cf)
    set(PROGREZZOR_PACKAGE_FORMAT --format=zip)
    set(PROGREZZOR_INSTALL_NOTES "${CMAKE_SOURCE_DIR}/packaging/INSTALL-windows.txt")
else()
    set(PROGREZZOR_PACKAGE_TARGET package-linux)
    set(PROGREZZOR_PACKAGE_NAME "Progrezzor-${PROJECT_VERSION}-linux-x86_64")
    set(PROGREZZOR_PACKAGE_FILE "Progrezzor-linux-x86_64.tar.gz")
    set(PROGREZZOR_TAR_FLAGS cfz)
    set(PROGREZZOR_PACKAGE_FORMAT --format=gnutar)
    set(PROGREZZOR_INSTALL_NOTES "${CMAKE_SOURCE_DIR}/packaging/INSTALL-linux.txt")
endif()
set(PROGREZZOR_PACKAGE_DIR "${CMAKE_BINARY_DIR}/package/${PROGREZZOR_PACKAGE_NAME}")
add_custom_target(${PROGREZZOR_PACKAGE_TARGET}
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${PROGREZZOR_PACKAGE_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PROGREZZOR_PACKAGE_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:Progrezzor_CLAP> ${PROGREZZOR_PACKAGE_DIR}/
    COMMAND ${CMAKE_COMMAND} -E copy_directory $<TARGET_FILE_DIR:Progrezzor_VST3>/../.. ${PROGREZZOR_PACKAGE_DIR}/Progrezzor.vst3
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/LICENSE ${PROGREZZOR_PACKAGE_DIR}/LICENSE.txt
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/CHANGELOG.md ${PROGREZZOR_PACKAGE_DIR}/
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/THIRD-PARTY-NOTICES.txt ${PROGREZZOR_PACKAGE_DIR}/
    COMMAND ${CMAKE_COMMAND} -E copy ${PROGREZZOR_INSTALL_NOTES} ${PROGREZZOR_PACKAGE_DIR}/INSTALL.txt
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/dist
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR}/package
            ${CMAKE_COMMAND} -E tar ${PROGREZZOR_TAR_FLAGS} ${CMAKE_SOURCE_DIR}/dist/${PROGREZZOR_PACKAGE_FILE} ${PROGREZZOR_PACKAGE_FORMAT} ${PROGREZZOR_PACKAGE_NAME}
    DEPENDS Progrezzor_CLAP Progrezzor_VST3
    COMMENT "Packaging dist/${PROGREZZOR_PACKAGE_FILE}"
    VERBATIM)
