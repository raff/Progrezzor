#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <limits>

namespace
{
const char* const keyNames[engine::kNumKeys] = { "C", "C#/Db", "D", "D#/Eb", "E", "F", "F#/Gb", "G", "G#/Ab", "A", "A#/Bb", "B" };

// The white keys of a keyboard play pad 1 to 7. -1 for the black keys.
constexpr int padOfPitchClass[12] = { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };

// A message from the host, kept so that the MIDI buffer can be emptied before the notes are written
struct InputMessage
{
    int position;
    int kind; // see below
    int note;
    int velocity;
};
enum { noteOnMessage, noteOffMessage, allNotesOffMessage };

constexpr int kLoopVelocity = 100;
constexpr double kFallbackBpm = 120.0;
}

ProgrezzorProcessor::ProgrezzorProcessor()
    // A stereo output from day one: it carries the preview sound, and some hosts (Bitwig) do not run a plug-in
    // without audio ports.
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    juce::StringArray keys, scales;
    for (const char* name : keyNames)
        keys.add (name);
    for (const auto& scale : engine::scales)
        scales.add (scale.name);

    // Defaults: C Major, and the Bright mood (the plainest)
    addParameter (keyParam = new juce::AudioParameterChoice ({ "key", 1 }, "Key", keys, 0));
    addParameter (scaleParam = new juce::AudioParameterChoice ({ "scale", 1 }, "Scale", scales, (int) engine::ScaleId::Major));
    addParameter (loopParam = new juce::AudioParameterBool ({ "loop", 1 }, "Play", true)); // added last: earlier projects keep theirs
    addParameter (lockParam = new juce::AudioParameterBool ({ "lock", 1 }, "Lock to scale", true));

    juce::StringArray moods, voices;
    for (const char* name : engine::moodNames)
        moods.add (name);
    for (const char* name : { "Soft Pad", "Strings", "Keys", "Organ", "Stab", "Pluck", "Off" }) // Off: no preview sound (the MIDI is the same)
        voices.add (name);
    addParameter (moodParam = new juce::AudioParameterChoice ({ "mood", 1 }, "Mood", moods, (int) engine::Mood::Bright));
    addParameter (voiceParam = new juce::AudioParameterChoice ({ "voice", 1 }, "Voice", voices, 0));
    addParameter (arpOnParam = new juce::AudioParameterBool ({ "arpOn", 1 }, "Arp", false));
    addParameter (arpStyleParam = new juce::AudioParameterChoice ({ "arpStyle", 1 }, "Arp Pattern", juce::StringArray { "Up", "Down", "Up/Down", "Random" }, 0));
    addParameter (arpRateParam = new juce::AudioParameterChoice ({ "arpRate", 1 }, "Arp Speed", juce::StringArray { "Slow", "Normal", "Fast", "Very fast" }, engine::kDefaultArpRate));
    addParameter (arpOctavesParam = new juce::AudioParameterInt ({ "arpOctaves", 1 }, "Arp Octaves", 1, engine::kMaxArpOctaves, 1));
    addParameter (arpStepsParam = new juce::AudioParameterInt ({ "arpSteps", 1 }, "Arp Rhythm Length", engine::kMinArpSteps, engine::kMaxArpSteps, 8));
    addParameter (arpHitsParam = new juce::AudioParameterInt ({ "arpHits", 1 }, "Arp Hits", 1, engine::kMaxArpSteps, 8));
    addParameter (arpRotationParam = new juce::AudioParameterInt ({ "arpRotation", 1 }, "Arp Rotation", 0, engine::kMaxArpSteps - 1, 0));
    addParameter (octaveParam = new juce::AudioParameterInt ({ "octave", 1 }, "Octave", -engine::kMaxGeneralOctave, engine::kMaxGeneralOctave, 0));
    addParameter (bassParam = new juce::AudioParameterBool ({ "bassNote", 1 }, "Bass Note", false)); // added last: earlier projects keep theirs
    addParameter (rhythmModeParam = new juce::AudioParameterBool ({ "arpRhythm", 1 }, "Chord Rhythm", false));
    addParameter (rhythmPatternParam = new juce::AudioParameterChoice ({ "rhythmPattern", 1 }, "Rhythm Pattern",
                                                                       juce::StringArray (engine::rhythmPatternNames, engine::kNumRhythmPatterns), 0));
    addParameter (rhythmSpeedParam = new juce::AudioParameterChoice ({ "rhythmSpeed", 1 }, "Rhythm Speed", juce::StringArray (engine::rhythmSpeedNames, engine::kNumRhythmSpeeds), 0));
    addParameter (rhythmLengthParam = new juce::AudioParameterChoice ({ "rhythmLength", 1 }, "Rhythm Length", juce::StringArray (engine::rhythmLengthNames, engine::kNumRhythmLengths), 1));

    // Where the chords usually sit: the user's own setting, the same in every project
    if (const auto settings = juce::parseXML (getSettingsFile()))
    {
        homeOctave.store (juce::jlimit (-engine::kMaxHomeOctave, engine::kMaxHomeOctave, settings->getIntAttribute ("homeOctave", 0)));
        learnHints.store (settings->getIntAttribute ("learnHints", 0) != 0);
        showTooltips.store (settings->getIntAttribute ("tooltips", 1) != 0);
        suggestInversions.store (settings->getIntAttribute ("suggestInversions", 1) != 0);
    }

    for (auto& variant : padVariant)
        variant.store (-1);
    for (auto& entry : history)
        entry.store (0);
}

engine::ArpSettings ProgrezzorProcessor::getArpSettings() const
{
    engine::ArpSettings settings;
    settings.on = arpOnParam->get() && ! rhythmModeParam->get();
    settings.style = arpStyleParam->getIndex();
    settings.rate = arpRateParam->getIndex();
    settings.octaves = arpOctavesParam->get();
    settings.steps = arpStepsParam->get();
    settings.hits = arpHitsParam->get();
    settings.rotation = arpRotationParam->get();
    return settings;
}

engine::RhythmSettings ProgrezzorProcessor::getRhythmSettings() const
{
    engine::RhythmSettings settings;
    settings.on = arpOnParam->get() && rhythmModeParam->get();
    settings.pattern = rhythmPatternParam->getIndex();
    settings.speed = rhythmSpeedParam->getIndex();
    settings.length = rhythmLengthParam->getIndex();
    settings.hits = arpHitsParam->get();
    settings.rotation = arpRotationParam->get();
    return settings;
}

void ProgrezzorProcessor::prepareToPlay (double newSampleRate, int)
{
    sampleRate = newSampleRate;
    synth.prepare (sampleRate);
    wasPlaying = false;
    wasHostPlaying = false;
    internalRun.store (false);
    loopIndex = -1;
    internalPpq = 0.0;
    // The host may have been off for a while. Whatever was sounding gets its note off in the first block.
    flushNeeded = true;
}

bool ProgrezzorProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet().isDisabled()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void ProgrezzorProcessor::pressPad (int pad)
{
    sendGuiEvent ({ engine::NoteTracker::padSource (pad), true, pad, {} });
}

void ProgrezzorProcessor::releasePad (int pad)
{
    sendGuiEvent ({ engine::NoteTracker::padSource (pad), false, pad, {} });
}

void ProgrezzorProcessor::pressBlock (int block, const engine::Chord& chord)
{
    sendGuiEvent ({ engine::NoteTracker::blockSource (block), true, -1, chord });
}

void ProgrezzorProcessor::releaseBlock (int block)
{
    sendGuiEvent ({ engine::NoteTracker::blockSource (block), false, -1, {} });
}

void ProgrezzorProcessor::sendGuiEvent (GuiEvent event)
{
    if (event.source < 0 || event.source >= engine::NoteTracker::kNumSources)
        return;

    const auto write = fifo.write (1);
    if (write.blockSize1 > 0)
        queue[(size_t) write.startIndex1] = event;
    else if (write.blockSize2 > 0)
        queue[(size_t) write.startIndex2] = event;
    // A full queue drops the event. 64 mouse presses within one audio block does not happen.
}

// One block of work. It is a class of its own only to keep processBlock readable: it holds the output and the
// position, and turns pad presses into notes.
class ProgrezzorProcessor::AudioWork
{
public:
    AudioWork (ProgrezzorProcessor& p, juce::AudioBuffer<float>& b, juce::MidiBuffer& m)
        : owner (p), buffer (b), midi (m), numSamples (b.getNumSamples())
    {
    }

    void render (int upTo)
    {
        if (upTo > cursor)
        {
            auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : buffer.getWritePointer (0);
            owner.synth.render (buffer.getWritePointer (0), right, cursor, upTo);
            cursor = upTo;
        }
    }

    // The sound and the MIDI follow every note on or off at the position it has
    void emit (const engine::NoteEvent& e, int position)
    {
        render (position);
        if (e.on)
        {
            midi.addEvent (juce::MidiMessage::noteOn (1, e.note, (juce::uint8) e.velocity), position);
            owner.synth.noteOn (e.note, e.velocity);
        }
        else
        {
            midi.addEvent (juce::MidiMessage::noteOff (1, e.note), position);
            owner.synth.noteOff (e.note);
        }
    }

    // The loop: which block sounds. Call it whenever the block, the user's hands or the key may have changed.
    void syncLoop (int position, bool again = false)
    {
        const auto& snapshot = owner.loopBuffer.read();
        bool wanted = owner.loopActive && owner.userHolds == 0 && owner.loopIndex >= 0 && owner.loopIndex < snapshot.count;
        if (wanted) // a block that Quick-text disabled (a chord outside the scale, with the lock on) keeps its time but is silent
        {
            const auto& block = snapshot.blocks[(size_t) owner.loopIndex];
            wanted = ! block.rest && ! (block.disabled && owner.lockParam->get() && ! engine::chordFitsScale (block.chord, (engine::ScaleId) owner.scaleParam->getIndex()));
        }
        const int loopSource = engine::NoteTracker::loopSource();

        if (! wanted)
        {
            releaseSource (loopSource, position);
            return;
        }

        const auto chord = snapshot.blocks[(size_t) owner.loopIndex].chord;
        const int tonic = owner.keyParam->getIndex();
        if (! again && owner.sourceActive[(size_t) loopSource] && owner.loopChord == chord && owner.loopTonic == tonic)
            return;

        owner.loopChord = chord;
        owner.loopTonic = tonic;
        pressSource (loopSource, -1, chord, kLoopVelocity, position);
    }

    // The host's position of the start of the loop's block `loopIndex`, for the block that is playing at `position`
    double loopBlockStart (int position) const
    {
        const auto& snapshot = owner.loopBuffer.read();
        double offset = 0.0;
        for (int i = 0; i < owner.loopIndex && i < snapshot.count; ++i)
            offset += snapshot.blocks[(size_t) i].beats;
        const double now = owner.bufferStartBeat + position * owner.bufferBeatsPerSample;
        const double cycles = std::floor ((now - offset) / snapshot.total + 1.0e-6);
        return offset + cycles * snapshot.total;
    }

    // The loop starts the block at its first sample
    void beginLoop (int index)
    {
        owner.loopIndex = index;
        nextChange = 0;
        // The arp of a block that the host has jumped away from (or into the middle of) starts over on the new grid
        bool stale = false;
        if (index >= 0)
        {
            const auto& slot = owner.arpSlots[(size_t) engine::NoteTracker::loopSource()];
            stale = slot.running && slot.onGrid && std::abs (slot.gridStart - loopBlockStart (0)) > 1.0e-6;
        }
        syncLoop (0, owner.loopPlan.startsOnLine || stale); // a block that starts here sounds anew, even with the same chord
    }

    // The loop goes through the block changes that fall before `position`
    void advanceLoop (int position)
    {
        while (nextChange < owner.loopPlan.numChanges && owner.loopPlan.changes[(size_t) nextChange].sample < position)
        {
            const auto change = owner.loopPlan.changes[(size_t) nextChange++];
            advanceArp (change.sample); // the steps before the change come first
            owner.loopIndex = change.index;
            syncLoop (change.sample, true); // every block change strikes the chord again, also when it is the same chord
        }
    }

    // `pad` is the pad that lights up (0 to 6), -1 for a chord that is not a pad
    void pressSource (int source, int pad, const engine::Chord& chord, int velocity, int position)
    {
        if (source < 0 || source >= engine::NoteTracker::kNumSources || pad >= kNumPads)
            return;

        // The same chord always has the same notes: only the key and the Oct buttons count
        const auto voicing = engine::voiceChord (chord, owner.keyParam->getIndex());
        const auto heard = engine::shiftedByOctaves (voicing, owner.getTotalOctave());

        if (pad >= 0)
            owner.recordPlayed (chord); // every chord played on a pad goes in the history

        pressingAgain = true; // the loop is not heard in between
        releaseSource (source, position); // a source that is pressed again lets go of its first pad
        pressingAgain = false;
        owner.sourcePad[(size_t) source] = pad;
        if (pad >= 0)
            ++owner.padHolds[(size_t) pad];
        const bool user = source != engine::NoteTracker::loopSource();
        if (user)
            ++owner.userHolds;
        owner.sourceActive[(size_t) source] = true;
        if (owner.blockArp.on || owner.blockRhythm.on)
            startArp (source, heard, velocity, position); // the arp: one note at a time
        else
            owner.tracker.press (source, heard, velocity, [&] (const engine::NoteEvent& e) { emit (e, position); });
        if (owner.blockBass) // the bass note lies under the chord (and under the arp) as long as the source is held
            owner.tracker.pressBass (source, engine::bassNote (chord, owner.keyParam->getIndex(), owner.getTotalOctave()), velocity,
                                     [&] (const engine::NoteEvent& e) { emit (e, position); });

        if (user) // what the user plays comes before the loop, which is silent as long as a hand is on a pad
            syncLoop (position);
    }

    void releaseSource (int source, int position)
    {
        if (source < 0 || source >= engine::NoteTracker::kNumSources || ! owner.sourceActive[(size_t) source])
            return;

        owner.sourceActive[(size_t) source] = false;
        owner.arpSlots[(size_t) source].running = false;
        owner.arpSlots[(size_t) source].offIn = -1.0;

        if (const int pad = owner.sourcePad[(size_t) source]; pad >= 0)
            --owner.padHolds[(size_t) pad];
        const bool user = source != engine::NoteTracker::loopSource();
        if (user)
            --owner.userHolds;
        owner.tracker.release (source, [&] (const engine::NoteEvent& e) { emit (e, position); });

        if (user && ! pressingAgain) // the last hand lets go: the loop is heard again
            syncLoop (position);
    }

    // Everything stops, the loop too. The loop is heard again at the next block change.
    void releaseEverything (int position)
    {
        owner.tracker.releaseAll ([&] (const engine::NoteEvent& e) { emit (e, position); });
        owner.padHolds.fill (0);
        owner.userHolds = 0;
        owner.sourceActive.fill (false);
        for (auto& slot : owner.arpSlots)
        {
            slot.running = false;
            slot.offIn = -1.0;
        }
    }

    // Learn mode: a key plays its own note, not a pad
    void pressNote (int source, int note, int velocity, int position)
    {
        if (source < 0 || source >= engine::NoteTracker::kNumSources)
            return;
        pressingAgain = true;
        releaseSource (source, position);
        pressingAgain = false;
        owner.sourcePad[(size_t) source] = -1;
        ++owner.userHolds;
        owner.sourceActive[(size_t) source] = true;
        engine::Voicing single;
        single.count = 1;
        single.notes[0] = note;
        owner.tracker.press (source, single, velocity, [&] (const engine::NoteEvent& e) { emit (e, position); });
    }

    // ---- The arp --------------------------------------------------------------------------------------------------
    // A source that holds a chord plays it one note at a time. The steps follow from the moment the chord started (a pad
    // pressed, a block of the loop begun), the notes from the step number alone, so the MIDI file has the same notes.

    // The first step is played at once
    void startArp (int source, const engine::Voicing& heard, int velocity, int position)
    {
        auto& slot = owner.arpSlots[(size_t) source];
        slot.settings = owner.blockArp;
        slot.pattern = slot.settings.pattern();
        slot.pool = engine::arpPool (heard, slot.settings.octaves);
        slot.rhythm = owner.blockRhythm.on; // chord rhythm: the whole chord at every strike of the pattern
        slot.rhythmSettings = owner.blockRhythm;
        slot.heard = heard;
        slot.stepBeats = slot.rhythm ? owner.blockRhythm.slotBeats() : slot.settings.stepBeats();
        slot.velocity = velocity;
        slot.step = 0;
        slot.running = true;
        slot.offIn = -1.0;
        slot.onGrid = source == engine::NoteTracker::loopSource() && owner.loopIndex >= 0;
        if (slot.onGrid)
        {
            // The step that is under way on the host's grid (step 0 when the block has just begun) plays at once
            const auto& snapshot = owner.loopBuffer.read();
            slot.gridStart = loopBlockStart (position);
            slot.blockLen = snapshot.blocks[(size_t) owner.loopIndex].beats;
            const double now = owner.bufferStartBeat + position * owner.bufferBeatsPerSample;
            slot.step = std::max<std::int64_t> (0, (std::int64_t) std::floor ((now - slot.gridStart) / slot.stepBeats + 1.0e-6));
        }
        playArpStep (source, (double) position);
    }

    void playArpStep (int source, double at)
    {
        auto& slot = owner.arpSlots[(size_t) source];
        const int position = juce::jlimit (0, std::max (0, numSamples - 1), (int) at);
        const auto emitNow = [&] (const engine::NoteEvent& e) { emit (e, position); };
        const double stepSamples = slot.stepBeats * owner.samplesPerBeat;

        owner.tracker.releaseNotes (source, emitNow); // the note of the step before, if it still sounds
        slot.offIn = -1.0;

        if (slot.rhythm)
        {
            const auto step = slot.step++;
            if (slot.rhythmSettings.isHit (step))
            {
                owner.tracker.pressNotes (source, slot.heard, slot.rhythmSettings.velocityFor (slot.velocity, step), emitNow);
                slot.offIn = at + slot.rhythmSettings.noteBeats (step) * owner.samplesPerBeat;
                slot.offBeat = owner.bufferStartBeat + slot.offIn * owner.bufferBeatsPerSample;
            }
        }
        else
        {
            const auto hit = engine::arpStep (slot.settings, slot.pattern, slot.pool, slot.step++);
            if (hit.play)
            {
                engine::Voicing single;
                single.count = 1;
                single.notes[0] = hit.note;
                owner.tracker.pressNotes (source, single, slot.velocity, emitNow);
                slot.offIn = at + engine::kArpGate * stepSamples;
                slot.offBeat = owner.bufferStartBeat + slot.offIn * owner.bufferBeatsPerSample;
            }
        }
        slot.dueIn = at + stepSamples;
        if (slot.onGrid) // the next step is on the host's grid, and it is only the block's if it starts before the block ends
        {
            const double stepBeats = slot.stepBeats;
            if ((double) slot.step * stepBeats >= slot.blockLen - 1.0e-9)
                slot.dueBeat = std::numeric_limits<double>::infinity();
            else
                slot.dueBeat = slot.gridStart + (double) slot.step * stepBeats;
            slot.dueIn = (slot.dueBeat - owner.bufferStartBeat) / owner.bufferBeatsPerSample;
        }
    }

    // Every step and every end of a note that falls before `position`, in the order they come
    void advanceArp (int position)
    {
        for (;;)
        {
            int bestSource = -1;
            double bestTime = (double) position;
            bool isEnd = false;
            for (int source = 0; source < engine::NoteTracker::kNumSources; ++source)
            {
                const auto& slot = owner.arpSlots[(size_t) source];
                if (! slot.running)
                    continue;
                if (slot.offIn >= 0.0 && slot.offIn < bestTime)
                {
                    bestSource = source;
                    bestTime = slot.offIn;
                    isEnd = true;
                }
                if (slot.dueIn < bestTime)
                {
                    bestSource = source;
                    bestTime = slot.dueIn;
                    isEnd = false;
                }
            }
            if (bestSource < 0)
                break;

            auto& slot = owner.arpSlots[(size_t) bestSource];
            if (isEnd)
            {
                const int at = juce::jlimit (0, std::max (0, numSamples - 1), (int) bestTime);
                owner.tracker.releaseNotes (bestSource, [&] (const engine::NoteEvent& e) { emit (e, at); });
                slot.offIn = -1.0;
            }
            else
                playArpStep (bestSource, bestTime);
        }
    }

    void finish()
    {
        render (numSamples);

        // What is left of the arp's timers is counted from the start of the next block
        for (auto& slot : owner.arpSlots)
            if (slot.running && ! slot.onGrid)
            {
                slot.dueIn -= (double) numSamples;
                if (slot.offIn >= 0.0)
                    slot.offIn -= (double) numSamples;
            }

        unsigned mask = 0;
        for (int pad = 0; pad < kNumPads; ++pad)
            if (owner.padHolds[(size_t) pad] > 0)
                mask |= 1u << pad;
        owner.lit.store (mask);
        owner.soundingCount.store (owner.tracker.soundingNotes());
        for (int half = 0; half < 2; ++half)
        {
            std::uint64_t bits = 0;
            for (int i = 0; i < 64; ++i)
                if (owner.tracker.isSounding (half * 64 + i))
                    bits |= (std::uint64_t) 1 << i;
            owner.soundingMask[(size_t) half].store (bits);
        }
        owner.playingBlock.store (owner.loopActive && owner.loopIndex >= 0 ? owner.loopIndex : -1);

        // Nothing louder than full scale can leave, whatever is played
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            auto* data = buffer.getWritePointer (channel);
            for (int i = 0; i < numSamples; ++i)
                data[i] = juce::jlimit (-1.0f, 1.0f, data[i]);
        }
    }

private:
    ProgrezzorProcessor& owner;
    juce::AudioBuffer<float>& buffer;
    juce::MidiBuffer& midi;
    int numSamples;
    int cursor = 0;
    int nextChange = 0;
    bool pressingAgain = false;
};

void ProgrezzorProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    // Read the incoming MIDI first and empty the buffer: the notes that go out are written into the same buffer, and
    // the notes that came in are not passed on.
    std::array<InputMessage, 256> input;
    int numInput = 0;
    for (const auto metadata : midi)
    {
        if (numInput == (int) input.size())
            break;

        const auto message = metadata.getMessage();
        if (message.isNoteOn())
            input[(size_t) numInput++] = { metadata.samplePosition, noteOnMessage, message.getNoteNumber(), message.getVelocity() };
        else if (message.isNoteOff())
            input[(size_t) numInput++] = { metadata.samplePosition, noteOffMessage, message.getNoteNumber(), 0 };
        else if (message.isAllNotesOff() || message.isAllSoundOff())
            input[(size_t) numInput++] = { metadata.samplePosition, allNotesOffMessage, 0, 0 };
    }
    midi.clear();

    AudioWork work (*this, buffer, midi);
    synth.setVoice (voiceParam->getIndex());
    if (voiceParam->getIndex() != PadSynth::Off)
        lastPreviewVoice.store (voiceParam->getIndex()); // what "Preview sound" in Settings turns back on

    if (flushNeeded)
    {
        work.releaseEverything (0);
        flushNeeded = false;
    }

    // The host's transport: playing or not, where it is (in quarter notes), its tempo and bars
    bool hostPlaying = wasHostPlaying;
    double bpm = kFallbackBpm;
    double startBeat = internalPpq;
    if (auto* transport = getPlayHead())
        if (const auto position = transport->getPosition())
        {
            hostPlaying = position->getIsPlaying();
            bpm = position->getBpm().orFallback (kFallbackBpm);
            startBeat = position->getPpqPosition().orFallback (internalPpq);
            if (const auto signature = position->getTimeSignature())
                if (signature->numerator > 0 && signature->denominator > 0)
                    hostBarBeats.store (4.0 * signature->numerator / signature->denominator);
        }
    if (bpm <= 0.0)
        bpm = kFallbackBpm;

    // Two clocks: the host's when it plays (the loop follows its position), otherwise the plug-in's own, which the Play
    // button starts from the beginning of the progression. When the host starts, it takes over.
    wasHostPlaying = hostPlaying;
    hostIsPlaying.store (hostPlaying);
    if (hostPlaying)
        internalRun.store (false);
    const bool ownClock = ! hostPlaying && internalRun.load();
    if (ownClock)
        startBeat = internalPpq; // 0 when it has just been started
    const bool playing = hostPlaying || ownClock;

    // Transport stop stops everything that sounds
    if (wasPlaying && ! playing)
    {
        work.releaseEverything (0);
        loopIndex = -1;
    }
    wasPlaying = playing;

    const double beatsPerSample = bpm / 60.0 / sampleRate;
    samplesPerBeat = sampleRate * 60.0 / bpm;
    blockArp = getArpSettings();
    blockRhythm = getRhythmSettings();
    blockBass = bassParam->get() && ! learnOn.load(); // in Learn mode the keys play their own notes
    if (learnOn.load())
        blockArp.on = blockRhythm.on = false; // in Learn mode a chord is played whole
    const int patternMode = blockArp.on ? 1 : blockRhythm.on ? 2 : 0;
    if (patternMode != patternModeWas) // Hold, Arp or Rhythm was chosen: everything that sounds stops, and starts again the other way
    {
        work.releaseEverything (0);
        patternModeWas = patternMode;
    }
    bufferStartBeat = startBeat;
    bufferBeatsPerSample = beatsPerSample;
    for (auto& slot : arpSlots) // the loop's arp is on the host's grid: where its steps fall in this block
        if (slot.running && slot.onGrid)
        {
            slot.dueIn = (slot.dueBeat - startBeat) / beatsPerSample;
            if (slot.offIn >= 0.0)
                slot.offIn = (slot.offBeat - startBeat) / beatsPerSample;
        }
    internalPpq = playing ? startBeat + buffer.getNumSamples() * beatsPerSample : 0.0;

    // The loop plays while the host plays and Play is on, or on its own clock when Play was pressed with the host stopped
    loopBuffer.update();
    loopActive = playing && (ownClock || loopParam->get()) && loopBuffer.read().count > 0 && ! learnOn.load();
    loopPlan = loopActive ? engine::planLoop (loopBuffer.read(), startBeat, beatsPerSample, buffer.getNumSamples()) : engine::LoopPlan();
    if (! loopActive)
        loopIndex = -1;

    // The mouse first (it arrived before this block), then the MIDI keyboard in the order of the buffer. What the user
    // plays comes before the loop, which is silent as long as a hand is on a pad.
    for (;;)
    {
        const auto read = fifo.read (1);
        const int index = read.blockSize1 > 0 ? read.startIndex1 : (read.blockSize2 > 0 ? read.startIndex2 : -1);
        if (index < 0)
            break;

        const auto event = queue[(size_t) index];
        if (event.down)
            work.pressSource (event.source, event.pad,
                              event.pad >= 0 ? getPadChord (event.pad) : event.chord,
                              kMouseVelocity, 0);
        else
            work.releaseSource (event.source, 0);
    }

    if (loopActive)
        work.beginLoop (loopPlan.startIndex);
    else
        work.syncLoop (0); // lets go of the loop's chord if Play was turned off or the progression emptied

    for (int i = 0; i < numInput; ++i)
    {
        const auto& message = input[(size_t) i];
        work.advanceLoop (message.position);
        work.advanceArp (message.position);

        if (message.kind == allNotesOffMessage)
        {
            work.releaseEverything (message.position);
            heldKeysLocal[0] = heldKeysLocal[1] = 0;
            continue;
        }

        // Which keys are down, for Learn mode
        if (message.note >= 0 && message.note < 128)
        {
            const std::uint64_t bit = (std::uint64_t) 1 << (message.note % 64);
            if (message.kind == noteOnMessage)
                heldKeysLocal[message.note / 64] |= bit;
            else
                heldKeysLocal[message.note / 64] &= ~bit;
        }

        if (learnOn.load()) // Learn: a key plays its own note
        {
            const int source = engine::NoteTracker::keySource (message.note);
            if (message.kind == noteOnMessage)
                work.pressNote (source, message.note, juce::jlimit (1, 127, message.velocity), message.position);
            else
                work.releaseSource (source, message.position);
            continue;
        }

        const int pad = padOfPitchClass[message.note % 12];
        if (pad < 0)
            continue; // a black key

        const int source = engine::NoteTracker::keySource (message.note);
        if (message.kind == noteOnMessage)
            work.pressSource (source, pad, getPadChord (pad),
                              juce::jlimit (1, 127, message.velocity), message.position);
        else
            work.releaseSource (source, message.position);
    }

    work.advanceLoop (buffer.getNumSamples());
    work.advanceArp (buffer.getNumSamples());
    heldKeys[0].store (heldKeysLocal[0]);
    heldKeys[1].store (heldKeysLocal[1]);
    work.finish();
}

void ProgrezzorProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    midi.clear(); // incoming notes are not passed on

    // Bypass must never leave a note hanging: everything that sounds gets its note off now
    AudioWork work (*this, buffer, midi);
    work.releaseEverything (0);
    synth.allNotesOff();
    loopIndex = -1;
    loopActive = false;
    internalPpq = 0.0;
    internalRun.store (false);
    work.finish();
    wasPlaying = false;
    wasHostPlaying = false;
}

void ProgrezzorProcessor::togglePlay()
{
    if (hostIsPlaying.load())
        loopParam->setValueNotifyingHost (loopParam->get() ? 0.0f : 1.0f);
    else
        internalRun.store (! internalRun.load());
}

void ProgrezzorProcessor::setProgression (engine::Progression newProgression)
{
    if (newProgression.size() > (size_t) engine::kMaxBlocks)
        newProgression.resize ((size_t) engine::kMaxBlocks);

    progression = std::move (newProgression);

    // The loop on the audio thread gets its own copy
    loopBuffer.writeSlot() = engine::makeSnapshot (progression);
    loopBuffer.publish();

    ++progressionVersion;
}

void ProgrezzorProcessor::editProgression (engine::Progression newProgression)
{
    if (newProgression.size() > (size_t) engine::kMaxBlocks)
        newProgression.resize ((size_t) engine::kMaxBlocks);
    if (newProgression == progression)
        return;

    undoStack.push_back (progression);
    if (undoStack.size() > kMaxUndo)
        undoStack.erase (undoStack.begin());
    redoStack.clear();
    setProgression (std::move (newProgression));
}

void ProgrezzorProcessor::undo()
{
    if (undoStack.empty())
        return;

    redoStack.push_back (progression);
    auto previous = std::move (undoStack.back());
    undoStack.pop_back();
    setProgression (std::move (previous));
}

void ProgrezzorProcessor::redo()
{
    if (redoStack.empty())
        return;

    undoStack.push_back (progression);
    auto next = std::move (redoStack.back());
    redoStack.pop_back();
    setProgression (std::move (next));
}

bool ProgrezzorProcessor::isBlockSilent (int index) const
{
    return index >= 0 && index < (int) progression.size()
        && engine::isBlockDisabled (progression[(size_t) index], (engine::ScaleId) scaleParam->getIndex(), lockParam->get());
}

void ProgrezzorProcessor::allowOutsideScale()
{
    // The silent blocks are allowed for good: they keep playing if the lock is turned on again (one step for Undo)
    auto allowed = progression;
    for (int i = 0; i < (int) allowed.size(); ++i)
        if (isBlockSilent (i))
            allowed[(size_t) i].disabled = false;
    editProgression (std::move (allowed));

    lockParam->beginChangeGesture();
    lockParam->setValueNotifyingHost (0.0f);
    lockParam->endChangeGesture();
}

void ProgrezzorProcessor::suggestProgression()
{
    // The chords of a suggestion are stored as distances from the tonic, so they follow the key by themselves
    auto suggestion = engine::suggest (getKey(), getMood(), (unsigned) random.nextInt(), lastSuggestion, suggestInversions.load());
    lastSuggestion = suggestion.index;

    // The library counts a bar as 4 beats; in the host's time signature a bar is what it is (3 beats in 3/4)
    const double barBeats = getHostBarBeats();
    for (auto& block : suggestion.blocks)
        block.beats = juce::jlimit (engine::kMinBlockBeats, engine::kMaxBlockBeats, block.beats / engine::kBeatsPerBar * barBeats);

    // Pad variants the user has chosen (B changed to B7) are used in the suggestion, also outside the scale
    std::array<engine::Chord, kNumPads> pads;
    for (int pad = 0; pad < kNumPads; ++pad)
        pads[(size_t) pad] = getPadChord (pad);
    suggestion.blocks = engine::withPadVariants (std::move (suggestion.blocks), (engine::ScaleId) scaleParam->getIndex(), pads);

    editProgression (engine::keepingLocked (progression, std::move (suggestion.blocks)));
    generatedVersion = progressionVersion.load();
}

bool ProgrezzorProcessor::suggestWouldOverwrite() const
{
    if (progressionVersion.load() == generatedVersion)
        return false;
    for (const auto& block : progression)
        if (! block.locked)
            return true;
    return false;
}

// A chord in one int: root, type and inversion
static int packChord (const engine::Chord& chord)
{
    return chord.root | ((int) chord.type << 4) | ((chord.inversion + 1) << 8) | ((chord.octave + engine::kMaxOctaveShift) << 12);
}

static engine::Chord unpackChord (int packed)
{
    engine::Chord chord;
    chord.root = engine::wrap12 (packed & 15);
    chord.type = (engine::ChordType) juce::jlimit (0, engine::kNumChordTypes - 1, (packed >> 4) & 15);
    chord.inversion = ((packed >> 8) & 15) - 1;
    chord.octave = juce::jlimit (-engine::kMaxOctaveShift, engine::kMaxOctaveShift, ((packed >> 12) & 15) - engine::kMaxOctaveShift);
    return chord;
}

void ProgrezzorProcessor::recordPlayed (const engine::Chord& chord)
{
    const unsigned n = historyCount.load (std::memory_order_relaxed);
    history[n % kHistorySize].store (packChord (chord), std::memory_order_relaxed);
    historyCount.store (n + 1, std::memory_order_release);
}

std::vector<engine::Chord> ProgrezzorProcessor::recentChords (int count) const
{
    const unsigned n = historyCount.load (std::memory_order_acquire);
    const int available = (int) std::min<unsigned> (n, (unsigned) kHistorySize);
    const int take = juce::jlimit (0, available, count);

    std::vector<engine::Chord> chords;
    for (unsigned i = n - (unsigned) take; i != n; ++i)
        chords.push_back (unpackChord (history[i % kHistorySize].load (std::memory_order_relaxed)));
    return chords;
}

engine::Chord ProgrezzorProcessor::getPadChord (int pad) const
{
    auto chord = engine::padChord ((engine::ScaleId) scaleParam->getIndex(), juce::jlimit (0, kNumPads - 1, pad));
    const int variant = padVariant[(size_t) juce::jlimit (0, kNumPads - 1, pad)].load();
    if (variant >= 0)
    {
        chord.type = (engine::ChordType) juce::jlimit (0, engine::kNumChordTypes - 1, variant >> 4);
        chord.inversion = (variant & 15) - 1;
    }
    return chord;
}

void ProgrezzorProcessor::setPadVariant (int pad, engine::ChordType type, int inversion)
{
    if (pad < 0 || pad >= kNumPads)
        return;

    padVariant[(size_t) pad].store ((int) type * 16 + juce::jlimit (engine::kAutoInversion, 4, inversion) + 1);
    ++variantVersion;
}

void ProgrezzorProcessor::clearPadVariant (int pad)
{
    if (pad < 0 || pad >= kNumPads)
        return;

    padVariant[(size_t) pad].store (-1);
    ++variantVersion;
}

// The user's own settings, in a file in the usual place for the system (or the folder in PROGREZZOR_SETTINGS_FOLDER)
juce::File ProgrezzorProcessor::getSettingsFile()
{
    const auto fromEnvironment = juce::SystemStats::getEnvironmentVariable ("PROGREZZOR_SETTINGS_FOLDER", {});
    const auto folder = fromEnvironment.isNotEmpty() ? juce::File (fromEnvironment)
                                                     : juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Progrezzor");
    return folder.getChildFile ("settings.xml");
}

void ProgrezzorProcessor::writeSettings() const
{
    juce::XmlElement xml ("ProgrezzorSettings");
    xml.setAttribute ("homeOctave", homeOctave.load());
    xml.setAttribute ("learnHints", learnHints.load() ? 1 : 0);
    xml.setAttribute ("tooltips", showTooltips.load() ? 1 : 0);
    xml.setAttribute ("suggestInversions", suggestInversions.load() ? 1 : 0);
    const auto file = getSettingsFile();
    if (file.getParentDirectory().createDirectory().wasOk())
        xml.writeTo (file);
}

void ProgrezzorProcessor::setHomeOctave (int octaves)
{
    homeOctave.store (juce::jlimit (-engine::kMaxHomeOctave, engine::kMaxHomeOctave, octaves));
    writeSettings();
}

void ProgrezzorProcessor::setShowTooltips (bool nowOn)
{
    showTooltips.store (nowOn);
    writeSettings();
}

void ProgrezzorProcessor::setLearnHints (bool nowOn)
{
    learnHints.store (nowOn);
    writeSettings();
}

void ProgrezzorProcessor::setSuggestInversions (bool nowOn)
{
    suggestInversions.store (nowOn);
    writeSettings();
}

void ProgrezzorProcessor::togglePreviewSound()
{
    const int wanted = voiceParam->getIndex() != PadSynth::Off ? (int) PadSynth::Off : lastPreviewVoice.load();
    voiceParam->beginChangeGesture();
    voiceParam->setValueNotifyingHost (voiceParam->convertTo0to1 ((float) wanted));
    voiceParam->endChangeGesture();
}

void ProgrezzorProcessor::setLearning (bool nowLearning)
{
    learnOn.store (nowLearning);
}

juce::File ProgrezzorProcessor::getExportFolder()
{
    const auto fromEnvironment = juce::SystemStats::getEnvironmentVariable ("PROGREZZOR_EXPORT_FOLDER", {});
    if (fromEnvironment.isNotEmpty())
        return juce::File (fromEnvironment);

    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Progrezzor");
}

juce::File ProgrezzorProcessor::saveMidi()
{
    constexpr int kTicksPerQuarter = 960;
    const auto key = getKey();

    // What the file is made from. The same again gives the same file.
    juce::String madeFrom = juce::String (key.tonic) + "/" + juce::String (getTotalOctave());
    const auto arp = getArpSettings();
    const auto rhythm = getRhythmSettings();
    if (rhythm.on)
        madeFrom << "/rhythm" << rhythm.pattern << "," << rhythm.speed << "," << rhythm.length << "," << rhythm.hits << "," << rhythm.rotation;
    if (arp.on)
        madeFrom << "/arp" << arp.style << "," << arp.rate << "," << arp.octaves << "," << arp.steps << "," << arp.hits << "," << arp.rotation;
    for (const auto& block : progression)
        madeFrom << (block.rest ? "|r" : (engine::isBlockDisabled (block, key.scale, lockParam->get()) ? "|-" : "|")) << block.chord.root << "," << (int) block.chord.type << "," << block.chord.inversion << "," << block.chord.octave << "," << juce::String (block.beats, 4);

    if (progression.empty())
        return {};
    if (madeFrom == lastExportKey && lastExport.existsAsFile())
        return lastExport;

    const auto ticks = [] (double beats) { return beats * kTicksPerQuarter; };

    juce::MidiMessageSequence sequence;
    for (const auto& note : engine::exportNotes (progression, key.tonic, key.scale, lockParam->get(), getTotalOctave(), engine::kCentreNote, arp, bassParam->get(), rhythm))
    {
        // All MIDI on channel 1. A note off comes before a note on at the same time, so that equal chords strike again.
        sequence.addEvent (juce::MidiMessage::noteOn (1, note.note, (juce::uint8) note.velocity).withTimeStamp (ticks (note.start)));
        sequence.addEvent (juce::MidiMessage::noteOff (1, note.note).withTimeStamp (ticks (note.start + note.length)));
    }
    sequence.addEvent (juce::MidiMessage::endOfTrack().withTimeStamp (ticks (engine::totalBeats (progression))));
    sequence.sort();
    sequence.updateMatchedPairs();

    juce::MidiFile midiFile; // a new one for every export
    midiFile.setTicksPerQuarterNote (kTicksPerQuarter);
    midiFile.addTrack (sequence);

    const auto folder = getExportFolder();
    const auto file = folder.getChildFile (juce::File::createLegalFileName (juce::String (engine::exportFileName (progression, key, arp.on, rhythm.on)))).getNonexistentSibling();

    bool written = false;
    if (folder.createDirectory().wasOk())
        if (juce::FileOutputStream stream (file); stream.openedOk())
        {
            stream.setPosition (0);
            stream.truncate();
            written = midiFile.writeTo (stream);
        }

    if (! written)
        return file.getParentDirectory().getChildFile ("not-written"); // does not exist

    lastExport = file;
    lastExportKey = madeFrom;
    return file;
}

// The key, scale, progression and pad variants as XML (version 2 added the progression, version 3 the pad variants, version 4 Lock to scale, version 5 disabled blocks, version 6 octaves, version 11 pauses).
void ProgrezzorProcessor::getStateInformation (juce::MemoryBlock& destination)
{
    juce::XmlElement xml ("Progrezzor");
    xml.setAttribute ("version", 11);
    xml.setAttribute ("key", keyParam->getIndex());
    xml.setAttribute ("scale", scaleParam->getIndex());
    xml.setAttribute ("loop", loopParam->get() ? 1 : 0);
    xml.setAttribute ("lock", lockParam->get() ? 1 : 0);
    xml.setAttribute ("mood", moodParam->getIndex());
    xml.setAttribute ("voice", voiceParam->getIndex());
    xml.setAttribute ("octave", octaveParam->get());
    xml.setAttribute ("bassNote", bassParam->get() ? 1 : 0);
    xml.setAttribute ("arpRhythm", rhythmModeParam->get() ? 1 : 0);
    xml.setAttribute ("rhythmPattern", rhythmPatternParam->getIndex());
    xml.setAttribute ("rhythmSpeed", rhythmSpeedParam->getIndex());
    xml.setAttribute ("rhythmLength", rhythmLengthParam->getIndex());
    xml.setAttribute ("arpOn", arpOnParam->get() ? 1 : 0);
    xml.setAttribute ("arpStyle", arpStyleParam->getIndex());
    xml.setAttribute ("arpRate", arpRateParam->getIndex());
    xml.setAttribute ("arpOctaves", arpOctavesParam->get());
    xml.setAttribute ("arpSteps", arpStepsParam->get());
    xml.setAttribute ("arpHits", arpHitsParam->get());
    xml.setAttribute ("arpRotation", arpRotationParam->get());

    for (int pad = 0; pad < kNumPads; ++pad)
        xml.setAttribute ("pad" + juce::String (pad), padVariant[(size_t) pad].load());

    for (const auto& block : progression)
    {
        auto* element = xml.createNewChildElement ("Block");
        element->setAttribute ("root", block.chord.root);
        element->setAttribute ("type", (int) block.chord.type);
        element->setAttribute ("inversion", block.chord.inversion);
        element->setAttribute ("octave", block.chord.octave);
        element->setAttribute ("beats", block.beats);
        element->setAttribute ("locked", block.locked ? 1 : 0);
        element->setAttribute ("disabled", block.disabled ? 1 : 0);
        element->setAttribute ("rest", block.rest ? 1 : 0);
    }
    copyXmlToBinary (xml, destination);
}

void ProgrezzorProcessor::setStateInformation (const void* data, int size)
{
    const auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName ("Progrezzor"))
        return;

    const auto set = [] (juce::AudioParameterChoice& parameter, int index)
    {
        parameter = juce::jlimit (0, parameter.choices.size() - 1, index);
    };
    set (*keyParam, xml->getIntAttribute ("key", 9));
    set (*scaleParam, xml->getIntAttribute ("scale", (int) engine::ScaleId::Minor));

    *loopParam = xml->getIntAttribute ("loop", 1) != 0; // projects saved before the loop have it on
    *lockParam = xml->getIntAttribute ("lock", 1) != 0; // and the lock
    set (*moodParam, xml->getIntAttribute ("mood", (int) engine::Mood::Melancholic)); // older projects: the mood they had
    set (*voiceParam, xml->getIntAttribute ("voice", 0));
    *arpOnParam = xml->getIntAttribute ("arpOn", 0) != 0;
    set (*arpStyleParam, xml->getIntAttribute ("arpStyle", 0));
    set (*arpRateParam, xml->getIntAttribute ("arpRate", engine::kDefaultArpRate));
    *arpOctavesParam = juce::jlimit (1, engine::kMaxArpOctaves, xml->getIntAttribute ("arpOctaves", 1));
    *arpStepsParam = juce::jlimit (engine::kMinArpSteps, engine::kMaxArpSteps, xml->getIntAttribute ("arpSteps", 8));
    *arpHitsParam = juce::jlimit (1, engine::kMaxArpSteps, xml->getIntAttribute ("arpHits", 8));
    *arpRotationParam = juce::jlimit (0, engine::kMaxArpSteps - 1, xml->getIntAttribute ("arpRotation", 0));
    *octaveParam = juce::jlimit (-engine::kMaxGeneralOctave, engine::kMaxGeneralOctave, xml->getIntAttribute ("octave", 0));
    *rhythmModeParam = xml->getIntAttribute ("arpRhythm", 0) != 0; // projects saved before the chord rhythm have the arp as it was
    set (*rhythmPatternParam, xml->getIntAttribute ("rhythmPattern", 0));
    set (*rhythmSpeedParam, xml->getIntAttribute ("rhythmSpeed", 0));
    set (*rhythmLengthParam, xml->getIntAttribute ("rhythmLength", 1));
    *bassParam = xml->getIntAttribute ("bassNote", 0) != 0; // projects saved before the bass note have it off

    for (int pad = 0; pad < kNumPads; ++pad)
    {
        const int variant = xml->getIntAttribute ("pad" + juce::String (pad), -1);
        padVariant[(size_t) pad].store (variant >= 0 && (variant >> 4) < engine::kNumChordTypes ? variant : -1);
    }
    ++variantVersion;

    engine::Progression loaded;
    for (auto* element : xml->getChildWithTagNameIterator ("Block"))
    {
        if ((int) loaded.size() >= engine::kMaxBlocks)
            break;

        engine::Block block;
        block.chord.root = engine::wrap12 (element->getIntAttribute ("root", 0));
        block.chord.type = (engine::ChordType) juce::jlimit (0, engine::kNumChordTypes - 1, element->getIntAttribute ("type", 0));
        block.chord.inversion = juce::jlimit (engine::kAutoInversion, 4, element->getIntAttribute ("inversion", engine::kAutoInversion));
        block.chord.octave = juce::jlimit (-engine::kMaxOctaveShift, engine::kMaxOctaveShift, element->getIntAttribute ("octave", 0));
        block.beats = juce::jlimit (engine::kMinBlockBeats, engine::kMaxBlockBeats, element->getDoubleAttribute ("beats", engine::kDefaultBlockBeats));
        block.locked = element->getIntAttribute ("locked", 0) != 0;
        block.disabled = element->getIntAttribute ("disabled", 0) != 0;
        block.rest = element->getIntAttribute ("rest", 0) != 0; // projects from before pauses have none
        loaded.push_back (block);
    }
    undoStack.clear(); // a loaded project starts with a clean history
    redoStack.clear();
    setProgression (std::move (loaded));

    // Tells the host that every parameter may have changed, so that it reads them all again (as SeqVerse does)
    updateHostDisplay (ChangeDetails().withProgramChanged (true));
}

juce::AudioProcessorEditor* ProgrezzorProcessor::createEditor()
{
    return new ProgrezzorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ProgrezzorProcessor();
}
