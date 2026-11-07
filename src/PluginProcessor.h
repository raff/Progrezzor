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
    juce::AudioParameterChoice* voiceParam = nullptr; // the preview sound: Pad, Strings, Keys, Organ, Stab, Pluck
    // The arpeggio (Arp button): everything is played one note at a time while it is on, and the MIDI file follows
    juce::AudioParameterBool* arpOnParam = nullptr;
    juce::AudioParameterChoice* arpStyleParam = nullptr;
    juce::AudioParameterChoice* arpRateParam = nullptr;
    juce::AudioParameterInt* arpOctavesParam = nullptr;
    juce::AudioParameterInt* arpStepsParam = nullptr;
    juce::AudioParameterInt* arpHitsParam = nullptr;
    juce::AudioParameterInt* arpRotationParam = nullptr;
    juce::AudioParameterInt* octaveParam = nullptr;   // the general register: -2 to +2 octaves for everything that sounds and is exported
    juce::AudioParameterBool* loopParam = nullptr; // the Play button: whether the progression loops while the host plays
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
    bool isPlayOn() const { return hostIsPlaying.load() ? loopParam->get() : internalRun.load(); }
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
    // The same for Capture, which replaces the whole bar: true if there is anything in it that did not come from Suggest or
    // Capture themselves
    bool captureWouldOverwrite() const;

    // The chords played on the pads (mouse and MIDI), newest last, at most `count`. Blocks and the loop are not in it.
    std::vector<engine::Chord> recentChords (int count) const;
    // Puts the last `count` played chords in the bar, one bar each. False if nothing has been played.
    bool capture (int count);

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
    // The arp as the parameters have it now. Safe to call from any thread.
    engine::ArpSettings getArpSettings() const;
    int getGeneralOctave() const { return octaveParam->get(); }

    // Where the chords usually sit (Settings): -2 to +2 octaves from middle C. It is the user's, kept in a file of its own,
    // and the same in every project, so it is not in the project's state. The Oct buttons count from there.
    int getHomeOctave() const { return homeOctave.load(); }
    void setHomeOctave (int octaves); // message thread: kept and written to the settings file
    int getTotalOctave() const { return juce::jlimit (-engine::kMaxTotalOctave, engine::kMaxTotalOctave, homeOctave.load() + octaveParam->get()); }
    static juce::File getSettingsFile();

    // Learn mode: the user plays the chords of the progression one after the other. While it is on the loop is silent and the arp
    // is off, so that what you play is what you hear. The session itself is kept by the editor; this holds the switch, the
    // user's choice about hints, and what has been played since the last look.
    bool isLearning() const { return learnOn.load(); }
    void setLearning (bool nowLearning);
    bool getLearnHints() const { return learnHints.load(); } // hints (the pad that glows, the notes on the keyboard) from the start
    void setLearnHints (bool nowOn); // message thread: kept in the settings file
    std::vector<engine::Chord> takeNewlyPlayed(); // the chords played on the pads (mouse and MIDI keys) since the last call
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
    };
    std::array<ArpSlot, engine::NoteTracker::kNumSources> arpSlots {};
    std::array<bool, engine::NoteTracker::kNumSources> sourceActive {}; // a source holds a chord (also in a rest of the arp)
    bool arpWasOn = false;
    double samplesPerBeat = 22050.0;
    engine::ArpSettings blockArp; // the arp as it is during this block

    // The loop (audio thread). The progression reaches it through a lock-free hand-over.
    engine::TripleBuffer<engine::LoopSnapshot> loopBuffer;
    engine::LoopPlan loopPlan;
    int loopIndex = -1;          // the block that plays now, -1 if none
    bool loopActive = false;     // the host plays, Play is on and there is a progression
    engine::Chord loopChord;     // the chord the loop is sounding, and the tonic it was voiced for
    int loopTonic = -1;
    int userHolds = 0;           // pads, keys and blocks held by the user; while there are any, the loop is silent
    double sampleRate = 44100.0;
    double internalPpq = 0.0;    // only used when the host does not tell its position
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
    int generatedVersion = -1; // progressionVersion right after the last Suggest or Capture
    juce::Random random;
    std::atomic<int> progressionVersion { 0 };

    std::atomic<unsigned> lit { 0 };
    std::atomic<int> soundingCount { 0 };
    std::atomic<int> homeOctave { 0 };
    std::atomic<bool> learnOn { false };
    std::atomic<bool> learnHints { false };
    unsigned learnCursor = 0;
    void writeSettings() const;
    std::array<std::atomic<std::uint64_t>, 2> soundingMask {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProgrezzorProcessor)
};
