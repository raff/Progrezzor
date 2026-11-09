#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

#include "PadSynth.h"
#include "engine/ChordEngine.h"
#include "engine/Library.h"
#include "engine/LoopPlan.h"
#include "engine/TripleBuffer.h"
#include "engine/NoteTracker.h"
#include "engine/ProgressionEdit.h"
#include "engine/MidiExport.h"
#include "engine/ChordRhythm.h"
#include "engine/QuickText.h"

// Milestone 2: seven pads that play chords as MIDI (channel 1) and as a built-in Pad sound.
//
// A pad can be pressed with the mouse (the editor puts the press in a lock-free queue) or with a white key on a MIDI
// keyboard (C to B in every octave play pad 1 to 7; black keys are ignored; the velocity is used). Incoming notes are
// not passed on. Every note that was started gets exactly one note off, see NoteTracker, also at transport stop, in
// bypass and when the host starts the plug-in anew.
class ProgrezzorProcessor : public juce::AudioProcessor
{
public:
    ProgrezzorProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    using AudioProcessor::processBlockBypassed;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Owned by the processor (addParameter). Read on the audio thread with getIndex() only.
    juce::AudioParameterChoice* keyParam = nullptr;
    juce::AudioParameterChoice* scaleParam = nullptr;
    juce::AudioParameterChoice* moodParam = nullptr;  // one of the 8 moods: it decides which progressions Suggest picks from
    juce::AudioParameterChoice* voiceParam = nullptr; // the preview sound: Soft Pad, Strings, Keys, Organ, Stab, Pluck, Off
    // The arpeggio (Arp button): everything is played one note at a time while it is on, and the MIDI file follows
    juce::AudioParameterBool* arpOnParam = nullptr;
    juce::AudioParameterChoice* arpStyleParam = nullptr;
    juce::AudioParameterChoice* arpRateParam = nullptr;
    juce::AudioParameterInt* arpOctavesParam = nullptr;
    juce::AudioParameterInt* arpStepsParam = nullptr;
    juce::AudioParameterInt* arpHitsParam = nullptr;
    juce::AudioParameterInt* arpRotationParam = nullptr;
    juce::AudioParameterInt* octaveParam = nullptr;   // the general register: -2 to +2 octaves for everything that sounds and is exported
    // Chord rhythm (the Rhythm choice of the arp panel). Hold = Arp off. Arp = arpOn with rhythmMode off. Rhythm = arpOn with
    // rhythmMode on. Hits and Rotation of the arp are also the Hits and Rotation of the Euclid pattern.
    juce::AudioParameterBool* rhythmModeParam = nullptr;
    juce::AudioParameterChoice* rhythmPatternParam = nullptr;
    juce::AudioParameterChoice* rhythmSpeedParam = nullptr;
    juce::AudioParameterChoice* rhythmLengthParam = nullptr;
    juce::AudioParameterChoice* keyboardModeParam = nullptr; // White keys (as standard) or Chord roots: which pad a MIDI key plays
    juce::AudioParameterChoice* voicingParam = nullptr; // Settings > Voicing: Root position (as standard) or Nearest inversion
    juce::AudioParameterBool* bassParam = nullptr;    // an extra bass note under every chord (Settings: Bass note), off as standard
    juce::AudioParameterBool* loopParam = nullptr; // no longer used for anything (the DAW always plays the loop); kept so that old projects open as before. Was: whether the progression loops while the host plays
    juce::AudioParameterBool* lockParam = nullptr; // Lock to scale (on by default)

    engine::Key getKey() const
    {
        return { keyParam->getIndex(), (engine::ScaleId) scaleParam->getIndex() };
    }

    // The editor (message thread) presses and releases pads through a queue that the audio thread empties.
    static constexpr int kNumPads = 7;
    static constexpr int kMouseVelocity = 100;
    void pressPad (int pad);
    void releasePad (int pad);

    // A block of the progression plays its chord while it is held with the mouse
    void pressBlock (int block, const engine::Chord& chord);
    void releaseBlock (int block);

    // Which pads are sounding now (bit 0 is pad 1), whether pressed by the mouse or from MIDI. For the editor.
    unsigned litPads() const { return lit.load(); }

    // The block of the progression that the loop plays now, -1 if it does not play. For the editor.
    int getPlayingBlock() const { return playingBlock.load(); }

    // Edit mode (the Edit button in the bar). While it is on, the loop is silent, so that nothing plays by itself while the
    // bar is being changed. Not saved: a project always opens in Play. For the editor.
    void setEditing (bool nowEditing) { editing.store (nowEditing); }
    bool isEditing() const { return editing.load(); }

    // The Play button. With the host playing it turns the loop on or off (the loop follows the host's position). With the host
    // stopped it starts or stops the plug-in's own clock, so the progression plays without the DAW running. For the editor.
    void togglePlay();
    // Play lights when the progression plays: with the DAW while it plays, otherwise on the plug-in's own clock
    bool isPlayOn() const { return hostIsPlaying.load() || internalRun.load(); }
    bool isHostPlaying() const { return hostIsPlaying.load(); }

    // The length of a bar in the host's time signature, in beats (4 until the host has told)
    double getHostBarBeats() const { return hostBarBeats.load(); }

    // Which notes sound now, for the little keyboard in the editor (bit 0 of the first number is note 0, of the second note 64)
    bool isNoteSounding (int note) const { return note >= 0 && note < 128 && ((soundingMask[(size_t) (note / 64)].load() >> (note % 64)) & 1u) != 0; }

    // The notes the plug-in has sent that are still on, for the tests
    int soundingNotes() const { return soundingCount.load(); }

    // ---- The progression (message thread only: the editor and the host's save and load) ----
    // Milestone 4 gives the audio thread a lock-free copy of it for the loop.
    const engine::Progression& getProgression() const { return progression; }
    void setProgression (engine::Progression newProgression);
    int getProgressionVersion() const { return progressionVersion.load(); } // goes up with each change, for the editor

    // Every change the user makes goes through here: the old progression is kept for Undo. (setProgression above is for
    // loading and for the live steps of a drag; it keeps nothing.)
    void editProgression (engine::Progression newProgression);
    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }
    void undo();
    void redo();

    // True if the block is silent now: Quick-text disabled it, Lock to scale is on and the chord is not in the scale
    bool isBlockSilent (int index) const;
    // The Allow button: such blocks are allowed (they play, also if the lock comes back) and Lock to scale is turned off (message thread)
    void allowOutsideScale();

    // Fills the bar with a whole new progression for the key and the mood. A different one each time. Locked blocks stay.
    void suggestProgression();
    // True if Suggest would throw away chords that did not come from Suggest itself (an empty bar, a bar of only locked blocks
    // and a bar that is still the last suggestion are replaced without asking)
    bool suggestWouldOverwrite() const;

    // The chords played on the pads (mouse and MIDI), newest last, at most `count`. Blocks and the loop are not in it.
    std::vector<engine::Chord> recentChords (int count) const;
    // How many chords have been played so far, and the ones played after that count (at most the last 32). Rec uses these.
    unsigned getPlayedCount() const { return historyCount.load (std::memory_order_acquire); }
    std::vector<engine::Chord> chordsPlayedSince (unsigned count) const { return recentChords ((int) std::min<unsigned> (getPlayedCount() - count, 32u)); }

    // ---- MIDI export (message thread) ----
    // The folder the files go in: a folder called Progrezzor in the user's music folder (or the one in the environment
    // variable PROGREZZOR_EXPORT_FOLDER). Files are never deleted, the DAW may read them later.
    static juce::File getExportFolder();

    // Writes the progression as a new MIDI file, named after its chords. Gives the file, or a file that does not exist if
    // it could not be written. Called again with the same chords and key, it gives the file written before (if it is
    // still there), so that dragging again does not fill the folder with copies.
    juce::File saveMidi();

    // ---- Pad variants (a pad can play another kind of chord on the same root) ----
    // The chord a pad plays now, in the current scale and with its variant. Safe to call from any thread.
    engine::Chord getPadChord (int pad) const;
    bool padHasVariant (int pad) const { return padVariant[(size_t) pad].load() >= 0; }
    void setPadVariant (int pad, engine::ChordType type, int inversion);
    void clearPadVariant (int pad);
    int getVariantVersion() const { return variantVersion.load(); }
    bool isLockedToScale() const { return lockParam->get(); }
    void turnLockOff(); // "More chords outside the key..." in the chord menus: Lock to scale off, so every chord is offered
    // The arp as the parameters have it now. Safe to call from any thread.
    // The pad that a MIDI key plays, -1 if none. White keys: C to B are pad 1 to 7. Chord roots: the pad whose chord has the key's
    // note as its root (also a black key, like Bb in D Minor), in every octave.
    int padForMidiNote (int note) const;
    static constexpr int kKeyboardWhiteKeys = 0, kKeyboardChordRoots = 1;
    engine::VoicingMode getVoicingMode() const { return (engine::VoicingMode) voicingParam->getIndex(); }
    engine::ArpSettings getArpSettings() const;   // .on: the arp (one note at a time) is the mode
    engine::RhythmSettings getRhythmSettings() const; // .on: chord rhythm is the mode
    int getGeneralOctave() const { return octaveParam->get(); }

    // Where the chords usually sit (Settings): -2 to +2 octaves from middle C. It is the user's, kept in a file of its own,
    // and the same in every project, so it is not in the project's state. The Oct buttons count from there.
    int getHomeOctave() const { return homeOctave.load(); }
    void setHomeOctave (int octaves); // message thread: kept and written to the settings file
    int getTotalOctave() const { return juce::jlimit (-engine::kMaxTotalOctave, engine::kMaxTotalOctave, homeOctave.load() + octaveParam->get()); }
    static juce::File getSettingsFile();

    // Learn mode: the user plays the chords of the progression one after the other, on the keys of a MIDI keyboard. While it is on
    // the loop is silent and the arp is off, and the keys play their own notes (they are not the pads), so that what you play is
    // what you hear. The session itself is kept by the editor; this holds the switch, the user's choice about hints, and which
    // keys are held.
    bool isLearning() const { return learnOn.load(); }
    void setLearning (bool nowLearning);
    bool getShowTooltips() const { return showTooltips.load(); } // tooltips on, as standard; kept for every project
    void setShowTooltips (bool nowOn);
    bool getLearnHints() const { return learnHints.load(); } // hints (the keys to hold) from the start
    void setLearnHints (bool nowOn); // message thread: kept in the settings file
    void togglePreviewSound(); // Settings: Preview sound. Off, or back to the sound chosen last. Message thread.
    int getLastPreviewVoice() const { return lastPreviewVoice.load(); }
    bool getSuggestInversions() const { return suggestInversions.load(); } // Suggest may use inversions (G/B), on as standard
    void setSuggestInversions (bool nowOn); // message thread: kept in the settings file
    bool isKeyHeld (int note) const { return note >= 0 && note < 128 && ((heldKeys[(size_t) (note / 64)].load() >> (note % 64)) & 1u) != 0; } // a key of the MIDI keyboard
    engine::Mood getMood() const { return (engine::Mood) moodParam->getIndex(); }

private:
    struct GuiEvent
    {
        int source;   // see NoteTracker
        bool down;
        int pad;      // 0 to 6 for a pad, -1 for a block
        engine::Chord chord; // the chord of a block
    };

    class AudioWork; // what happens in one block, see the .cpp

    void sendGuiEvent (GuiEvent);

    static constexpr int kQueueSize = 64;
    juce::AbstractFifo fifo { kQueueSize };
    std::array<GuiEvent, kQueueSize> queue {};

    // Audio thread only
    engine::NoteTracker tracker;
    std::array<int, engine::NoteTracker::kNumSources> sourcePad {}; // which pad a source is holding, -1 for a block
    std::array<int, kNumPads> padHolds {};                           // how many sources hold each pad
    PadSynth synth;
    bool wasPlaying = false;
    bool wasHostPlaying = false;

    // The arp, audio thread only: one slot for every source that holds a chord, stepping through its notes
    struct ArpSlot
    {
        bool running = false;
        engine::ArpSettings settings;
        engine::EuclidPattern pattern;
        engine::ArpPool pool;
        std::int64_t step = 0;
        double dueIn = 0.0;   // samples from the start of this block to the next step
        double offIn = -1.0;  // samples to the end of the note that sounds, -1 if none
        int velocity = 100;
        // The loop's arp is counted on the host's grid (quarter notes), not by adding step lengths: step k of a block is at
        // gridStart + k * stepBeats, and only steps before gridStart + blockLen belong to the block. Pads count from the press.
        bool onGrid = false;
        double gridStart = 0.0; // the host's position of the start of the block
        double blockLen = 0.0;  // beats
        double dueBeat = 0.0;   // where the next step is (infinity: no more steps in this block)
        double offBeat = -1.0;  // where the note that sounds ends, -1 if none
        bool rhythm = false;    // chord rhythm: every strike is the whole chord (`heard`), not a note of the pool
        engine::RhythmSettings rhythmSettings;
        engine::Voicing heard;
        double stepBeats = 0.5; // the length of a step (or a slot of the rhythm)
    };
    std::array<ArpSlot, engine::NoteTracker::kNumSources> arpSlots {};
    std::array<bool, engine::NoteTracker::kNumSources> sourceActive {}; // a source holds a chord (also in a rest of the arp)
    int patternModeWas = 0; // 0 hold, 1 arp, 2 rhythm: when it changes, everything that sounds stops
    double samplesPerBeat = 22050.0;
    double bufferStartBeat = 0.0;     // the host's position (quarter notes) at the first sample of this block
    double bufferBeatsPerSample = 0.0;
    engine::ArpSettings blockArp; // the arp as it is during this block
    engine::RhythmSettings blockRhythm; // and the chord rhythm
    engine::VoicingMode blockVoicing = engine::VoicingMode::RootPosition; // as it is during this block
    bool blockBass = false;       // the bass note as it is during this block

    // The loop (audio thread). The progression reaches it through a lock-free hand-over.
    engine::TripleBuffer<engine::LoopSnapshot> loopBuffer;
    engine::LoopPlan loopPlan;
    int loopIndex = -1;          // the block that plays now, -1 if none
    bool loopActive = false;     // the host plays, Play is on and there is a progression
    engine::Chord loopChord;     // the chord the loop is sounding, and the tonic it was voiced for
    int loopTonic = -1;
    int userHolds = 0;           // pads, keys and blocks held by the user (they play on top of the loop, which never goes quiet for them)
    double sampleRate = 44100.0;
    double internalPpq = 0.0;    // only used when the host does not tell its position, or its position stands still while it plays
    double lastHostBeat = -1.0e300; // the host's position at the last block, to see if it is stuck
    double lastHostAdvance = 0.0;  // how far the host should have moved in that block (beats)
    int stuckBlocks = 0;         // blocks in a row where a playing host gave the same position
    std::atomic<int> playingBlock { -1 };
    std::atomic<bool> editing { false };
    std::atomic<bool> internalRun { false }; // the plug-in's own clock runs (not saved: a project opens stopped)
    std::atomic<bool> hostIsPlaying { false };
    std::atomic<double> hostBarBeats { engine::kBeatsPerBar };
    bool flushNeeded = true;

    // The chords played on the pads: written by the audio thread, read by the message thread. A chord is packed in an int.
    static constexpr int kHistorySize = 32;
    std::array<std::atomic<int>, kHistorySize> history {};
    std::atomic<unsigned> historyCount { 0 };
    void recordPlayed (const engine::Chord&);

    // -1 for no variant, otherwise type * 16 + (inversion + 1)
    std::array<std::atomic<int>, kNumPads> padVariant {};
    std::atomic<int> variantVersion { 0 };

    // Message thread
    juce::File lastExport;
    juce::String lastExportKey; // what the file was made from: key and every block
    static constexpr size_t kMaxUndo = 100;
    std::vector<engine::Progression> undoStack, redoStack;
    engine::Progression progression;
    int lastSuggestion = -1;
    int generatedVersion = -1; // progressionVersion right after the last Suggest
    juce::Random random;
    std::atomic<int> progressionVersion { 0 };

    std::atomic<unsigned> lit { 0 };
    std::atomic<int> soundingCount { 0 };
    std::atomic<int> homeOctave { 0 };
    std::atomic<bool> learnOn { false };
    std::atomic<bool> learnHints { false };
    std::atomic<bool> showTooltips { true };
    std::atomic<bool> suggestInversions { true };
    std::atomic<int> lastPreviewVoice { 0 }; // the sound that was chosen last (not Off), for the Preview sound switch in Settings
    std::array<std::atomic<std::uint64_t>, 2> heldKeys {}; // the keys of the MIDI keyboard that are down
    std::uint64_t heldKeysLocal[2] = { 0, 0 };              // audio thread
    void writeSettings() const;
    std::array<std::atomic<std::uint64_t>, 2> soundingMask {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProgrezzorProcessor)
};
