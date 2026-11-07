// Headless test of the editor: builds the real editor (no window, no host) next to a real processor, plays pads the way
// the mouse and a MIDI keyboard do, and checks that they light up. Optionally writes PNG snapshots of the editor, so
// that the layout can be looked at:
//
//   ProgrezzorUiTest [snapshot-prefix]      exit code 0 = all checks passed

#include "../src/PluginEditor.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>

namespace
{
int failures = 0;

bool near (double a, double b) { return std::abs (a - b) < 1.0e-9; }

void check (bool ok, const std::string& what)
{
    std::printf ("  %s: %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (! ok)
        ++failures;
}

// A host's transport that plays or does not, at a position given in quarter notes
class FakeTransport : public juce::AudioPlayHead
{
public:
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setIsPlaying (playing);
        info.setPpqPosition (quarters);
        info.setBpm (120.0);
        info.setTimeSignature (TimeSignature { 4, 4 });
        return info;
    }

    bool playing = false;
    double quarters = 0.0;
};

void saveSnapshot (juce::Component& editor, const std::string& path)
{
    const auto image = editor.createComponentSnapshot (editor.getLocalBounds());
    juce::File file (path);
    file.deleteFile();
    if (auto stream = file.createOutputStream())
        juce::PNGImageFormat().writeImageToStream (image, *stream);
}

void runBlock (ProgrezzorProcessor& processor, juce::MidiBuffer midi = {})
{
    juce::AudioBuffer<float> audio (2, 512);
    processor.processBlock (audio, midi);
}

void settle (int milliseconds = 120)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil (milliseconds);
}
}

int main (int argc, char** argv)
{
    const juce::ScopedJuceInitialiser_GUI gui;

    // The tests never touch the user's own settings file
    const auto settingsFolder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("progrezzor-test-settings-" + juce::String (juce::Time::currentTimeMillis()));
    setenv ("PROGREZZOR_SETTINGS_FOLDER", settingsFolder.getFullPathName().toRawUTF8(), 1);

    ProgrezzorProcessor processor;
    processor.setPlayConfigDetails (0, 2, 48000.0, 512);
    processor.prepareToPlay (48000.0, 512);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    const std::string prefix = argc > 1 ? argv[1] : "";
    const auto snapshot = [&] (const char* name)
    {
        if (! prefix.empty())
            saveSnapshot (*editor, prefix + "_" + name + ".png");
    };

    std::printf ("start\n");
    settle();
    runBlock (processor);
    check (processor.litPads() == 0, "no pad is lit at rest");
    snapshot ("rest");

    std::printf ("mouse\n");
    // The editor is not on screen, so the pad is pressed through the processor like the pad's mouse handler does
    processor.pressPad (2);
    runBlock (processor);
    settle();
    check (processor.litPads() == (1u << 2), "pad 3 is lit while pressed");
    check (processor.soundingNotes() == 3, "three notes sound");
    snapshot ("pad3_lit");
    processor.releasePad (2);
    runBlock (processor);
    settle();
    check (processor.litPads() == 0, "pad 3 goes out when released");
    check (processor.soundingNotes() == 0, "no notes sound");

    std::printf ("MIDI keyboard\n");
    juce::MidiBuffer keys;
    keys.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 90), 0);  // C: pad 1
    keys.addEvent (juce::MidiMessage::noteOn (1, 67, (juce::uint8) 90), 10); // G: pad 5
    keys.addEvent (juce::MidiMessage::noteOn (1, 61, (juce::uint8) 90), 20); // black key: ignored
    runBlock (processor, keys);
    settle();
    check (processor.litPads() == ((1u << 0) | (1u << 4)), "pads 1 and 5 are lit from MIDI");
    snapshot ("midi_two_lit");

    juce::MidiBuffer release;
    release.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
    release.addEvent (juce::MidiMessage::noteOff (1, 67), 0);
    runBlock (processor, release);
    check (processor.litPads() == 0 && processor.soundingNotes() == 0, "everything is off after the release");

    std::printf ("key and scale\n");
    *processor.keyParam = 2;   // D
    *processor.scaleParam = 1; // Major
    settle (200);
    snapshot ("d_major");
    *processor.keyParam = 10;  // A#/Bb
    *processor.scaleParam = 0;
    settle (200);
    snapshot ("bb_minor");

    std::printf ("Suggest and the bar\n");
    *processor.keyParam = 9;
    *processor.scaleParam = 0;
    settle();
    check (processor.getProgression().empty(), "the bar is empty at first");
    snapshot ("bar_empty");

    auto* suggest = editor->findChildWithID ("suggest");
    check (suggest != nullptr, "there is a Suggest button");
    if (auto* button = dynamic_cast<juce::Button*> (suggest))
        button->triggerClick();
    settle (200);
    const auto first = processor.getProgression();
    check (! first.empty(), "Suggest fills the bar");
    bool inScale = true;
    for (const auto& block : first)
        inScale &= engine::chordFitsScale (block.chord, engine::ScaleId::Minor);
    check (inScale, "the suggestion fits the scale");
    bool onPads = true;
    for (int i = 0; i < 50; ++i)
    {
        processor.suggestProgression();
        for (const auto& block : processor.getProgression())
            onPads &= engine::isPadChord (block.chord, engine::ScaleId::Minor);
    }
    check (onPads, "every chord in a suggestion is one of the pads");
    snapshot ("bar_suggested");

    // Clicking Suggest again gives another progression
    bool different = false;
    for (int i = 0; i < 20 && ! different; ++i)
    {
        processor.suggestProgression();
        const auto next = processor.getProgression();
        different = next.size() != first.size();
        for (size_t b = 0; ! different && b < next.size(); ++b)
            different = next[b].chord != first[b].chord;
    }
    check (different, "another Suggest gives another progression");

    // A block plays its chord while held
    processor.pressBlock (1, processor.getProgression()[1].chord);
    runBlock (processor);
    check (processor.soundingNotes() >= 3, "a block plays its chord");
    processor.releaseBlock (1);
    runBlock (processor);
    check (processor.soundingNotes() == 0, "and stops when released");

    // The progression follows the key: same chords, other names
    const auto before = processor.getProgression();
    *processor.keyParam = 2;
    settle (200);
    check (processor.getProgression().size() == before.size() && processor.getProgression()[0].chord == before[0].chord,
           "a new key does not change the stored progression");
    snapshot ("bar_in_d");

    std::printf ("the loop\n");
    FakeTransport transport;
    processor.setPlayHead (&transport);
    *processor.keyParam = 9;
    *processor.scaleParam = 0;
    processor.setProgression ({ { { 0, engine::ChordType::Minor, engine::kAutoInversion }, 4.0, false },
                                { { 8, engine::ChordType::Major, engine::kAutoInversion }, 4.0, false },
                                { { 3, engine::ChordType::Major, engine::kAutoInversion }, 4.0, false },
                                { { 10, engine::ChordType::Major, engine::kAutoInversion }, 4.0, false } });
    transport.playing = false;
    runBlock (processor);
    check (processor.getPlayingBlock() == -1, "no block is playing while the host is stopped");
    transport.playing = true;
    transport.quarters = 5.0; // the second bar
    runBlock (processor);
    settle (200);
    check (processor.getPlayingBlock() == 1, "the second block plays at beat 5");
    check (processor.soundingNotes() == 3, "and its chord sounds");
    snapshot ("loop_playing");
    *processor.loopParam = false;
    runBlock (processor);
    check (processor.getPlayingBlock() == -1 && processor.soundingNotes() == 0, "Play off: nothing plays");
    snapshot ("loop_off");
    *processor.loopParam = true;
    transport.playing = false;
    runBlock (processor);

    // Play with the DAW stopped: the plug-in's own clock plays the progression from its start
    transport.quarters = 0.0;
    check (! processor.isPlayOn(), "Play is off while the DAW is stopped");
    processor.togglePlay();
    runBlock (processor);
    check (processor.isPlayOn() && processor.getPlayingBlock() == 0 && processor.soundingNotes() == 3, "Play plays the first chord on its own");
    for (int i = 0; i < 200; ++i) // about four beats at 120 bpm
        runBlock (processor);
    check (processor.getPlayingBlock() == 1, "and goes on to the next chord on its own clock");
    processor.togglePlay();
    runBlock (processor);
    check (! processor.isPlayOn() && processor.getPlayingBlock() == -1 && processor.soundingNotes() == 0, "Play again stops it, nothing hangs");

    // When the DAW starts, it takes over
    processor.togglePlay();
    runBlock (processor);
    transport.playing = true;
    transport.quarters = 9.0;
    runBlock (processor);
    check (processor.isHostPlaying() && processor.getPlayingBlock() == 2, "the DAW takes over when it starts");
    transport.playing = false;
    runBlock (processor);
    check (! processor.isPlayOn() && processor.soundingNotes() == 0, "and when it stops, everything is quiet");
    processor.setPlayHead (nullptr);

    std::printf ("same chord twice\n");
    {
        FakeTransport t2;
        processor.setPlayHead (&t2);
        const engine::Chord b7 { 7, engine::ChordType::Dom7, engine::kAutoInversion };
        processor.setProgression ({ { b7, 4.0, false }, { b7, 4.0, false }, { { 0, engine::ChordType::Minor, engine::kAutoInversion }, 4.0, false } });
        t2.playing = true;
        int noteOns = 0, firstNote = -1, bars = 0;
        for (double q = 0.0; q < 11.9; q += 512.0 * 2.0 / 48000.0) // 120 bpm, 512 samples per block, three bars (the wrap at 12 is left out)
        {
            t2.quarters = q;
            juce::AudioBuffer<float> audio (2, 512);
            juce::MidiBuffer out;
            processor.processBlock (audio, out);
            for (const auto m : out)
                if (m.getMessage().isNoteOn())
                {
                    ++noteOns;
                    if (q < 4.0 && firstNote < 0)
                        firstNote = m.getMessage().getNoteNumber();
                    if (q > 3.9 && q < 4.1)
                        ++bars;
                }
        }
        check (bars == 4, "the second B7 strikes all four notes again at the bar line");
        check (noteOns == 4 + 4 + 3, "every block strikes its chord: 4 + 4 + 3 notes in three bars");
        t2.playing = false;
        runBlock (processor);
        processor.setPlayHead (nullptr);
    }

    std::printf ("editing\n");
    {
        auto& ui = *static_cast<ProgrezzorEditor*> (editor.get());
        auto& bar = ui.getBar();
        const auto mk = [] (int root, double beats)
        {
            engine::Block b;
            b.chord = { root, engine::ChordType::Minor, engine::kAutoInversion };
            b.beats = beats;
            return b;
        };
        const auto roots = [&]
        {
            std::string r;
            for (const auto& b : processor.getProgression())
                r += std::to_string (b.chord.root) + " ";
            return r;
        };
        processor.setProgression ({ mk (0, 4), mk (2, 4), mk (4, 4), mk (7, 4) });
        settle();
        check (bar.numBlocks() == 4, "four blocks");

        // With Play off (the start) the bar can be edited
        check (bar.isEditing(), "the bar starts editable, Play is off");
        // Play on: a block only plays, nothing in the bar can be changed
        bar.setEditing (false);
        const auto mouse = [] (juce::Component& c, juce::Point<float> where, bool right)
        {
            return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), where, right ? juce::ModifierKeys (juce::ModifierKeys::rightButtonModifier) : juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier),
                                     0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c, juce::Time::getCurrentTime(), where, juce::Time::getCurrentTime(), 1, false);
        };
        const auto inPlay = processor.getProgression();
        bar.getBlock (1)->mouseDown (mouse (*bar.getBlock (1), { 30.0f, 30.0f }, true)); // right-click
        bar.getBlock (1)->mouseUp (mouse (*bar.getBlock (1), { 30.0f, 30.0f }, true));
        bar.getBlock (1)->mouseDown (mouse (*bar.getBlock (1), { 30.0f, 30.0f }, false)); // a click: plays
        runBlock (processor);
        check (processor.soundingNotes() == 3, "in Play a click on a block plays its chord");
        bar.getBlock (1)->mouseUp (mouse (*bar.getBlock (1), { 30.0f, 30.0f }, false));
        runBlock (processor);
        check (processor.soundingNotes() == 0 && bar.getSelected() == -1, "and lets go of it, without selecting it");
        check (processor.getProgression() == inPlay, "right-click and clicks change nothing in Play");
        ui.getPad (3).onPress();
        ui.getPad (3).onRelease();
        check (processor.getProgression() == inPlay, "a pad never replaces a block in Play");
        runBlock (processor);

        // The Play button decides: on is Play, off can be edited
        processor.togglePlay();
        bar.updateButtons();
        check (! bar.isEditing() && processor.isPlayOn(), "Play on: no editing");
        processor.togglePlay();
        bar.updateButtons();
        check (bar.isEditing() && processor.isEditing() && ! processor.isPlayOn(), "Play off: the bar can be edited");
        settle (50);
        snapshot ("edit_on");

        // Lock a block, then Suggest keeps it
        bar.getBlock (1)->onToggleLock();
        check (processor.getProgression()[1].locked && processor.canUndo(), "the lock toggles and can be undone");
        settle();
        snapshot ("locked_block");
        bool kept = true;
        for (int i = 0; i < 30; ++i)
        {
            processor.suggestProgression();
            const auto& p = processor.getProgression();
            kept &= p.size() > 1 && p[1].locked && p[1].chord.root == 2 && p[1].chord.type == engine::ChordType::Minor;
        }
        check (kept, "a locked block stays when Suggest replaces the rest");

        // Delete, undo, redo
        processor.setProgression ({ mk (0, 4), mk (2, 4), mk (4, 4), mk (7, 4) });
        settle();
        bar.getBlock (1)->onDelete();
        check (roots() == "0 4 7 ", "right-click deletes the block");
        processor.undo();
        check (roots() == "0 2 4 7 ", "undo brings it back");
        processor.redo();
        check (roots() == "0 4 7 ", "redo deletes it again");
        processor.undo();
        settle();

        // The length: dragging the right edge, one undo step for the whole drag
        const auto undoBefore = processor.canUndo();
        bar.getBlock (0)->onResizeStart();
        bar.getBlock (0)->onResizeDrag (40);
        bar.getBlock (0)->onResizeDrag (400);
        const double during = processor.getProgression()[0].beats;
        settle (60);
        snapshot ("resize_long");
        bool allInside = true;
        for (int b = 0; b < bar.numBlocks(); ++b)
            allInside &= bar.getBlock (b)->getBounds().getRight() <= bar.getTrayBounds().getRight();
        check (allInside, "a block pulled long makes the others smaller, so that nothing runs out of the bar");
        bar.getBlock (0)->onResizeEnd();
        check (during > 4.0 && near (processor.getProgression()[0].beats, during), "dragging the edge makes the block longer");
        check (near (std::fmod (during, engine::kMinBlockBeats), 0.0), "in steps of half a bar");
        processor.undo();
        check (near (processor.getProgression()[0].beats, 4.0), "one undo undoes the whole drag");
        check (undoBefore, "(there was history before)");
        settle();

        // Move and copy
        bar.getBlock (0)->onMoveEnd ({ bar.getBlock (3)->getBounds().getRight() + 5, bar.getBlock (3)->getBounds().getCentreY() }, false);
        check (roots() == "2 4 7 0 ", "a block is dragged to the end");
        processor.undo();
        bar.getBlock (3)->onMoveEnd ({ bar.getBlock (0)->getBounds().getX() - 5, bar.getBlock (0)->getBounds().getCentreY() }, true);
        check (roots() == "7 0 2 4 7 ", "Ctrl-drag puts a copy in front");
        processor.undo();
        settle();

        // A pad pulled into the bar is inserted where it is let go
        const auto screenOf = [&] (int x) { return bar.localPointToGlobal (juce::Point<int> (x, bar.getBlock (0)->getBounds().getCentreY())); };
        ui.getPad (4).onDragEnd (screenOf (bar.getBlock (1)->getBounds().getRight() + 2)); // pad 5 in A minor is Em: root 7
        check (processor.getProgression().size() == 5 && processor.getProgression()[2].chord.root == 7, "a pad dropped in the bar is inserted");
        processor.undo();

        // A pad pressed (not dragged) never replaces anything, also when a block is selected
        settle();
        const int blocksBefore = bar.numBlocks();
        const auto untouched = processor.getProgression();
        bar.getBlock (2)->onSelect();
        check (bar.getSelected() == 2, "a click selects a block");
        ui.getPad (3).onPress();
        ui.getPad (3).onRelease();
        check (processor.getProgression() == untouched, "a pad click does not replace the selected block");
        bar.clearSelection();

        // A pad dragged onto the middle of a block replaces it, and the block lights up on the way
        {
            const auto middle = [&] (int i) { return bar.localPointToGlobal (bar.getBlock (i)->getBounds().getCentre()); };
            check (bar.blockAtScreenPoint (middle (2)) == 2, "the middle of a block is a place to replace");
            ui.getPad (3).onDragStart();
            ui.getPad (3).onDragMove (middle (2));
            settle (100);
            snapshot ("drag_replace");
            ui.getPad (3).onDragEnd (middle (2));
            check (processor.getProgression()[2].chord.root == 5 && (int) processor.getProgression().size() == blocksBefore,
                   "a pad dragged onto a block replaces it");
            processor.undo();
        }

        // Suggest asks before it replaces chords the user made, and not again while the bar is still a suggestion
        {
            const auto mine = processor.getProgression();
            check (processor.suggestWouldOverwrite(), "Suggest would overwrite the user's chords");
            ui.getSuggestButton().triggerClick();
            settle (50);
            check (processor.getProgression() == mine, "Suggest does not change the bar before the answer");
            check (bar.getReplaceButton().isVisible() && bar.getNotice().contains ("Replace"), "it asks in the bar");
            snapshot ("suggest_asks");
            bar.getReplaceButton().triggerClick();
            settle (50);
            check (processor.getProgression() != mine, "Replace puts in the suggestion");
            check (! processor.suggestWouldOverwrite(), "the next Suggest does not ask");
            ui.getSuggestButton().triggerClick();
            settle (50);
            check (! bar.getReplaceButton().isVisible(), "and no question is shown");
            processor.setProgression (mine);
            settle (50);
        }

        // Capture
        for (int pad : { 0, 0, 3, 4, 5 })
        {
            processor.pressPad (pad);
            runBlock (processor);
            processor.releasePad (pad);
            runBlock (processor);
        }
        check (processor.capture (4), "capture finds chords");
        check (roots() == "0 5 7 8 " && near (processor.getProgression()[0].beats, 4.0), "the last 4 played chords, one bar each");
        processor.undo();
        check (processor.capture (8), "capture 8");
        check (processor.getProgression().size() == 8 && processor.getProgression()[3].chord == processor.getProgression()[4].chord,
               "the same chord twice in a row counts twice");
        snapshot ("captured");
        // A long progression wraps into several rows
        {
            const auto beforeMany = processor.getProgression();
            for (int n = 0; n < 14; ++n)
                processor.editProgression (engine::withInserted (processor.getProgression(), n, processor.getPadChord (n % 7), n % 5 == 4 ? 8.0 : 4.0));
            settle();
            snapshot ("many_blocks");
            processor.setProgression (beforeMany);
            settle();
        }
        processor.undo();

        // With Play off (editing) the loop is silent, and Play brings it back
        FakeTransport t3;
        processor.setPlayHead (&t3);
        *processor.loopParam = false;
        processor.setProgression ({ mk (0, 4), mk (2, 4) });
        t3.playing = true;
        t3.quarters = 1.0;
        runBlock (processor);
        check (processor.getPlayingBlock() == -1 && processor.soundingNotes() == 0, "in Edit the loop is silent");
        *processor.loopParam = true;
        runBlock (processor);
        check (processor.getPlayingBlock() == 0 && processor.soundingNotes() == 3, "back in Play the loop plays again");
        t3.playing = false;
        runBlock (processor);
        processor.setPlayHead (nullptr);
        processor.setProgression ({});
        bar.setEditing (false);
        check (! processor.isEditing() && bar.getSelected() == -1, "leaving Edit drops the selection");
    }

    std::printf ("pad variants\n");
    {
        auto& ui = *static_cast<ProgrezzorEditor*> (editor.get());
        const auto progressionBefore = processor.getProgression();
        check (! processor.padHasVariant (0) && processor.getPadChord (0).type == engine::ChordType::Minor, "pad 1 starts as a minor chord");
        processor.setPadVariant (0, engine::ChordType::Min7, engine::kAutoInversion);
        settle (200);
        processor.pressPad (0);
        runBlock (processor);
        check (processor.soundingNotes() == 4, "a variant plays its own chord (four notes)");
        processor.releasePad (0);
        runBlock (processor);
        check (processor.getProgression() == progressionBefore, "a new variant does not change the bar");
        snapshot ("variant");

        juce::MemoryBlock saved;
        processor.getStateInformation (saved);
        ProgrezzorProcessor other;
        other.setStateInformation (saved.getData(), (int) saved.getSize());
        check (other.padHasVariant (0) && other.getPadChord (0).type == engine::ChordType::Min7 && ! other.padHasVariant (1),
               "the variants come back from a saved project");
        check (! other.canUndo(), "a loaded project starts without undo history");

        processor.clearPadVariant (0);
        check (processor.getPadChord (0).type == engine::ChordType::Minor, "back to the pad's own chord");
        juce::ignoreUnused (ui);
    }

    std::printf ("MIDI export\n");
    {
        auto& ui = *static_cast<ProgrezzorEditor*> (editor.get());
        const juce::File folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("progrezzor_ui_test_export");
        folder.deleteRecursively();
        setenv ("PROGREZZOR_EXPORT_FOLDER", folder.getFullPathName().toRawUTF8(), 1);
        *processor.keyParam = 9;
        *processor.scaleParam = 0;
        const auto mk = [] (int root, engine::ChordType type, double beats)
        {
            engine::Block b;
            b.chord = { root, type, engine::kAutoInversion };
            b.beats = beats;
            return b;
        };
        processor.setProgression ({});
        check (! processor.saveMidi().existsAsFile(), "nothing is written for an empty bar");

        processor.setProgression ({ mk (0, engine::ChordType::Minor, 4), mk (8, engine::ChordType::Major, 4),
                                    mk (3, engine::ChordType::Major, 4), mk (10, engine::ChordType::Major, 4) });
        const auto file = processor.saveMidi();
        check (file.existsAsFile() && file.getFileName() == "Am-F-C-G.mid", "the file is named after the chords: Am-F-C-G.mid");
        check (file.getParentDirectory() == folder, "and lies in the export folder");

        juce::MidiFile read;
        juce::FileInputStream in (file);
        check (in.openedOk() && read.readFrom (in) && read.getNumTracks() == 1, "the file is a valid MIDI file with one track");
        int ons = 0, offs = 0, wrongChannel = 0;
        double lastOff = 0.0;
        if (auto* track = read.getTrack (0))
            for (auto* e : *track)
            {
                const auto& m = e->message;
                if (m.isNoteOn())
                    ++ons;
                if (m.isNoteOff())
                {
                    ++offs;
                    lastOff = std::max (lastOff, m.getTimeStamp());
                }
                if ((m.isNoteOn() || m.isNoteOff()) && m.getChannel() != 1)
                    ++wrongChannel;
            }
        int metaEvents = 0, tempoOrSignature = 0, endMarks = 0;
        if (auto* track = read.getTrack (0))
            for (auto* e : *track)
            {
                const auto& m = e->message;
                if (m.isTempoMetaEvent() || m.isTimeSignatureMetaEvent())
                    ++tempoOrSignature;
                if (m.isEndOfTrackMetaEvent())
                    ++endMarks;
                if (m.isMetaEvent())
                    ++metaEvents;
            }
        check (tempoOrSignature == 0, "the file has no tempo and no time signature, so the DAW keeps its own");
        check (metaEvents == endMarks && endMarks == 1, "only notes and the end of the track");
        check (ons == 12 && offs == 12 && wrongChannel == 0, "twelve notes, each with its off, all on channel 1");
        check (std::abs (lastOff - 16.0 * read.getTimeFormat()) < 1.0, "the last note ends after four bars");

        check (processor.saveMidi() == file, "the same chords again give the same file, no copies");
        processor.setProgression ({ mk (0, engine::ChordType::Minor, 4), mk (0, engine::ChordType::Minor, 4) });
        const auto second = processor.saveMidi();
        check (second.getFileName() == "Am-Am.mid" && file.existsAsFile(), "other chords give another file, and the first stays");
        juce::MidiFile again;
        juce::FileInputStream in2 (second);
        again.readFrom (in2);
        int struck = 0;
        if (auto* track = again.getTrack (0))
            for (auto* e : *track)
                if (e->message.isNoteOn())
                    ++struck;
        check (struck == 6, "the same chord twice is struck twice in the file");

        processor.setProgression ({ mk (0, engine::ChordType::Minor, 4), mk (8, engine::ChordType::Major, 4),
                                    mk (3, engine::ChordType::Major, 4), mk (10, engine::ChordType::Major, 4) });
        settle();
        const auto dragged = ui.getBar().getDragTile().makeFile ? ui.getBar().getDragTile().makeFile() : juce::File();
        check (dragged.existsAsFile() && dragged.getFileName().startsWith ("Am-F-C-G") && ui.getBar().getDragTile().makeFile() == dragged, "the Drag tile hands over a file (and the same one again)");
        ui.getSaveButton().triggerClick();
        settle (50);
        check (ui.getSaveButton().getButtonText() == "Saved", "Save MIDI says Saved");
        snapshot ("export");
        settle (1800);
        check (ui.getSaveButton().getButtonText() == "Save MIDI", "and goes back to its name");
        folder.deleteRecursively();
    }

    std::printf ("Lock to scale\n");
    {
        auto& ui = *static_cast<ProgrezzorEditor*> (editor.get());
        *processor.keyParam = 9;
        *processor.scaleParam = 0; // A minor
        processor.setProgression ({});
        settle();
        check (processor.isLockedToScale() && ui.getLockButton().getToggleState(), "Lock to scale is on at first");

        // The lock off: the gear lists every kind of chord; an outside block is only marked while the lock is on
        const engine::Block outside { { 1, engine::ChordType::Major, engine::kAutoInversion }, 4.0, false };
        const engine::Block inside { { 0, engine::ChordType::Minor, engine::kAutoInversion }, 4.0, false };
        processor.setProgression ({ inside, outside });
        settle (150);
        check (! ui.getBar().getBlock (0)->isOutsideScale() && ui.getBar().getBlock (1)->isOutsideScale(), "a chord outside the scale is marked while the lock is on");
        snapshot ("outside_marked");
        const auto kept = processor.getProgression();
        ui.getLockButton().triggerClick();
        settle (150);
        check (! processor.isLockedToScale(), "the lock button turns the lock off");
        check (! ui.getBar().getBlock (1)->isOutsideScale(), "nothing is marked with the lock off");
        check (processor.getProgression() == kept, "the lock never changes a block");
        *processor.keyParam = 2;
        ui.getLockButton().triggerClick();
        settle (150);
        check (processor.isLockedToScale() && processor.getProgression() == kept, "a new key and the lock on again leave the blocks as they are");
        *processor.keyParam = 9;
        settle (150);
        processor.setProgression ({});
        settle (50);
    }

    std::printf ("Quick-text\n");
    {
        auto& ui = *static_cast<ProgrezzorEditor*> (editor.get());
        auto& bar = ui.getBar();
        const auto click = [] (juce::Button& button)
        {
            button.triggerClick(); // the click is posted to the message loop
            settle (40);
        };
        *processor.keyParam = 9;
        *processor.scaleParam = 0; // A minor
        *processor.lockParam = true;
        processor.setProgression ({});
        bar.setEditing (false);
        settle (100);

        check (! bar.getQuickTextButton().isVisible(), "the pencil is hidden in Play");
        bar.setEditing (true);
        check (bar.getQuickTextButton().isVisible(), "the pencil shows in Edit");
        click(bar.getQuickTextButton());
        check (bar.isInTextMode() && bar.getTextEditor().isVisible() && bar.getTextEditor().getText().isEmpty(), "the bar becomes an empty text line");
        snapshot ("quicktext_empty");

        // Enter uses the text
        bar.getTextEditor().setText ("Am F C G:2", false);
        settle (50);
        snapshot ("quicktext_typed");
        const auto padsBefore = processor.getPadChord (0);
        bar.applyQuickText();
        const auto& p = processor.getProgression();
        check (! bar.isInTextMode() && p.size() == 4 && near (p[3].beats, 8.0) && near (p[0].beats, 4.0), "Enter makes the blocks and leaves the text");
        check (p[1].chord.root == 8 && p[2].chord.root == 3 && p[3].chord.root == 10, "the chords are the ones typed");
        check (processor.getPadChord (0) == padsBefore && ! processor.padHasVariant (0), "the pads are not touched");
        settle (100);
        check (bar.numBlocks() == 4 && bar.getBlock (0)->isVisible(), "the blocks are back");
        check (bar.getNotice().isEmpty() && ! bar.getAllowButton().isVisible(), "no message when everything is in the scale");

        // The text shows the blocks exactly
        click(bar.getQuickTextButton());
        check (bar.getTextEditor().getText() == "Am F C G:2", "the text shows the blocks as they are");
        bar.getTextEditor().setText ("Am*2 F:0.5*2 C:2", false);
        bar.applyQuickText();
        check (p.size() == 5 && p[1].chord.root == 0 && near (p[2].beats, 2.0) && near (p[3].beats, 2.0) && near (p[4].beats, 8.0), "* and : make blocks");
        click(bar.getQuickTextButton());
        check (bar.getTextEditor().getText() == "Am Am F:0.5 F:0.5 C:2", "the shortcut is shown as separate blocks");

        // A word that cannot be read is marked, the rest is used, and Esc takes it all back
        bar.getTextEditor().setText ("Am Xq F", false);
        bar.applyQuickText();
        settle (50);
        check (bar.isInTextMode() && processor.getProgression().size() == 2, "a typo does not stop the rest");
        check (bar.getNotice().contains ("Xq"), "the message names the word");
        snapshot ("quicktext_typo");
        bar.cancelQuickText();
        check (! bar.isInTextMode() && processor.getProgression().size() == 5, "Esc brings back the bar as it was");

        click(bar.getQuickTextButton());
        bar.getTextEditor().setText ("Xq", false);
        bar.applyQuickText();
        check (bar.isInTextMode() && processor.getProgression().size() == 5, "text with nothing readable leaves the bar alone");
        bar.cancelQuickText();

        // Esc without changes, and leaving Edit
        click(bar.getQuickTextButton());
        bar.getTextEditor().setText ("G", false);
        bar.cancelQuickText();
        check (processor.getProgression().size() == 5 && ! bar.isInTextMode(), "Esc throws the text away");
        click(bar.getQuickTextButton());
        bar.setEditing (false);
        check (! bar.isInTextMode() && processor.getProgression().size() == 5, "leaving Edit closes the text");
        bar.setEditing (true);

        // A chord outside the scale becomes a silent, marked block until it is allowed
        click(bar.getQuickTextButton());
        bar.getTextEditor().setText ("Am E7 C", false);
        bar.applyQuickText();
        settle (100);
        check (processor.getProgression().size() == 3 && processor.getProgression()[1].disabled, "E7 is disabled");
        check (processor.getProgression()[1].chord.type == engine::ChordType::Dom7 && processor.getProgression()[1].chord.root == 7, "and still exactly E7");
        check (bar.getBlock (1)->isOutsideScale() && processor.isBlockSilent (1) && ! processor.isBlockSilent (0), "it is marked and silent");
        check (bar.getNotice().startsWith ("E7") && bar.getAllowButton().isVisible(), "a short message with an Allow button");
        snapshot ("quicktext_disabled");

        // It keeps its time in the loop but makes no sound; a block that is held does not sound either
        bar.setEditing (false);
        FakeTransport quickTransport;
        processor.setPlayHead (&quickTransport);
        quickTransport.playing = true;
        quickTransport.quarters = 5.0; // the second block
        runBlock (processor);
        check (processor.getPlayingBlock() == 1 && processor.soundingNotes() == 0, "the loop passes the disabled block in silence");
        quickTransport.quarters = 9.0;
        runBlock (processor);
        check (processor.getPlayingBlock() == 2 && processor.soundingNotes() == 3, "and plays the next one on time");
        quickTransport.quarters = 5.0;
        runBlock (processor);
        bar.getBlock (1)->onPress();
        runBlock (processor);
        check (processor.soundingNotes() == 0, "a click on it makes no sound");
        bar.getBlock (1)->onRelease();

        // Allow turns the lock off and it plays at once
        click(bar.getAllowButton());
        runBlock (processor);
        settle (100);
        check (! processor.isLockedToScale() && ! processor.isBlockSilent (1), "Allow turns the lock off");
        check (processor.soundingNotes() == 4, "and E7 plays right away");
        check (bar.getNotice().isEmpty() && ! bar.getAllowButton().isVisible(), "the message goes away");
        quickTransport.playing = false;
        runBlock (processor);
        processor.setPlayHead (nullptr);

        // With the lock off, nothing is disabled; the lock back on silences the chord again
        bar.setEditing (true);
        click(bar.getQuickTextButton());
        bar.getTextEditor().setText ("Am E7 Bb", false);
        bar.applyQuickText();
        check (! processor.getProgression()[1].disabled && ! processor.getProgression()[2].disabled, "with the lock off nothing is disabled");
        *processor.lockParam = true;
        settle (100);
        check (! processor.isBlockSilent (1) && ! processor.isBlockSilent (2), "blocks that were allowed keep playing when the lock comes back");

        // The disabled flag is part of the project
        click(bar.getQuickTextButton());
        bar.getTextEditor().setText ("Am B7", false);
        bar.applyQuickText();
        processor.editProgression (engine::withLockToggled (processor.getProgression(), 0));
        juce::MemoryBlock saved;
        processor.getStateInformation (saved);
        ProgrezzorProcessor other;
        other.setStateInformation (saved.getData(), (int) saved.getSize());
        check (other.getProgression().size() == 2 && ! other.getProgression()[0].disabled && other.getProgression()[1].disabled, "the disabled blocks come back from a saved project");
        check (other.getProgression()[0].locked, "and the lock of a block");

        // The 🔒 of a block survives a text that leaves it unchanged
        click(bar.getQuickTextButton());
        bar.applyQuickText();
        check (processor.getProgression()[0].locked && processor.getProgression()[1].disabled, "text that is not changed changes nothing");

        // Suggest and Capture close the text
        click(bar.getQuickTextButton());
        check (bar.isInTextMode(), "the text is open");
        bar.cancelQuickText();
        bar.setEditing (false);
        processor.setProgression ({});
        settle (50);
    }

    std::printf ("Octaves\n");
    {
        auto& ui = *static_cast<ProgrezzorEditor*> (editor.get());
        auto& bar = ui.getBar();
        *processor.keyParam = 9;
        *processor.scaleParam = 0;
        *processor.lockParam = true;
        processor.setProgression ({});
        bar.setEditing (false);
        settle (100);

        // The pads have no octave: they play and are dragged as they are
        processor.editProgression (engine::withInserted (processor.getProgression(), 0, processor.getPadChord (0), 4.0));
        processor.editProgression (engine::withInserted (processor.getProgression(), 1, processor.getPadChord (1), 4.0));
        settle (100);
        check (processor.getPadChord (0).octave == 0 && processor.getProgression()[0].chord.octave == 0, "a pad and the block made from it are in the normal register");

        // The arrows of a block (Edit only)
        bar.setEditing (true);
        settle (100);
        bar.getBlock (0)->onOctave (1);
        bar.getBlock (1)->onOctave (-1);
        settle (100);
        check (processor.getProgression()[0].chord.octave == 1 && processor.getProgression()[1].chord.octave == -1, "the arrows move one block each");
        check (bar.getBlock (1)->getOctave() == -1 && bar.getBlock (0)->getOctave() == 1, "and the block shows it");
        check (processor.getPadChord (0).octave == 0 && processor.getPadChord (1).octave == 0, "the pads are not touched");
        bar.getBlock (0)->setForceHover (true);
        settle (50);
        snapshot ("octave_hover");
        bar.getBlock (0)->setForceHover (false);
        snapshot ("octave_blocks");
        for (int i = 0; i < 5; ++i)
            bar.getBlock (0)->onOctave (1);
        check (processor.getProgression()[0].chord.octave == engine::kMaxOctaveShift, "two octaves is as far as it goes");
        processor.undo(); // the steps past the limit changed nothing, so they are not in the history
        check (processor.getProgression()[0].chord.octave == 1 && processor.getProgression()[1].chord.octave == -1, "Undo takes back the last step");

        // The loop plays the moved chord
        bar.setEditing (false);
        FakeTransport octaveTransport;
        processor.setPlayHead (&octaveTransport);
        octaveTransport.playing = true;
        const auto loopLowest = [&] (double quarters)
        {
            octaveTransport.quarters = quarters;
            juce::MidiBuffer out;
            juce::AudioBuffer<float> audio (2, 512);
            processor.processBlock (audio, out);
            int lowest = 999;
            for (const auto m : out)
                if (m.getMessage().isNoteOn())
                    lowest = std::min (lowest, m.getMessage().getNoteNumber());
            return lowest;
        };
        const int highBlock = loopLowest (0.0);
        const int lowBlock = loopLowest (4.0);
        octaveTransport.playing = false;
        runBlock (processor);
        processor.setPlayHead (nullptr);
        check (highBlock > lowBlock + 6, "the loop plays the high block high and the low block low");

        // The project keeps it
        juce::MemoryBlock saved;
        processor.getStateInformation (saved);
        ProgrezzorProcessor other;
        other.setStateInformation (saved.getData(), (int) saved.getSize());
        check (other.getProgression()[0].chord.octave == 1 && other.getProgression()[1].chord.octave == -1,
               "the octaves of the blocks come back from a saved project");

        processor.setProgression ({});
        settle (50);
    }

    std::printf ("general octave and a steady register\n");
    {
        *processor.keyParam = 9;
        *processor.scaleParam = 0;
        *processor.octaveParam = 0;
        processor.setProgression ({ { { 0, engine::ChordType::Minor, engine::kAutoInversion }, 4.0, false },
                                    { { 8, engine::ChordType::Major, engine::kAutoInversion }, 4.0, false },
                                    { { 3, engine::ChordType::Major, engine::kAutoInversion }, 4.0, false },
                                    { { 10, engine::ChordType::Major, engine::kAutoInversion }, 4.0, false } });
        settle (50);

        const auto notesOf = [&] (juce::MidiBuffer& buffer)
        {
            std::vector<int> notes;
            for (const auto m : buffer)
                if (m.getMessage().isNoteOn())
                    notes.push_back (m.getMessage().getNoteNumber());
            std::sort (notes.begin(), notes.end());
            return notes;
        };
        const auto padNotes = [&] (int pad)
        {
            juce::MidiBuffer out;
            juce::AudioBuffer<float> audio (2, 512);
            processor.pressPad (pad);
            processor.processBlock (audio, out);
            auto notes = notesOf (out);
            juce::MidiBuffer off;
            processor.releasePad (pad);
            processor.processBlock (audio, off);
            return notes;
        };

        // The pads: an octave up moves every note exactly 12 semitones
        const auto atZero = padNotes (0);
        *processor.octaveParam = 1;
        const auto atOne = padNotes (0);
        *processor.octaveParam = -1;
        const auto atMinusOne = padNotes (0);
        *processor.octaveParam = 0;
        bool exact = atZero.size() == 3 && atOne.size() == 3 && atMinusOne.size() == 3;
        for (size_t i = 0; exact && i < 3; ++i)
            exact = atOne[i] == atZero[i] + 12 && atMinusOne[i] == atZero[i] - 12;
        check (exact, "Oct+ and Oct- move the pads exactly an octave");

        // A note that is held when the register changes is let go of with the note that was started
        {
            juce::MidiBuffer out;
            juce::AudioBuffer<float> audio (2, 512);
            processor.pressPad (0);
            processor.processBlock (audio, out);
            const auto started = notesOf (out);
            *processor.octaveParam = 2;
            processor.releasePad (0);
            juce::MidiBuffer off;
            processor.processBlock (audio, off);
            std::vector<int> ended;
            for (const auto m : off)
                if (m.getMessage().isNoteOff())
                    ended.push_back (m.getMessage().getNoteNumber());
            std::sort (ended.begin(), ended.end());
            *processor.octaveParam = 0;
            check (started == ended && started.size() == 3, "changing the register while a pad is held leaves no note hanging");
        }

        // The loop always plays the progression in the same place, whatever was played on the pads before, and it is the
        // place that the MIDI file has
        const auto loopFirstBlock = [&] (int padBefore)
        {
            if (padBefore >= 0)
            {
                padNotes (padBefore);
                padNotes (padBefore);
            }
            FakeTransport loopTransport;
            processor.setPlayHead (&loopTransport);
            loopTransport.playing = false;
            runBlock (processor);
            loopTransport.playing = true;
            loopTransport.quarters = 0.0;
            juce::MidiBuffer out;
            juce::AudioBuffer<float> audio (2, 512);
            processor.processBlock (audio, out);
            const auto notes = notesOf (out);
            loopTransport.playing = false;
            runBlock (processor);
            processor.setPlayHead (nullptr);
            return notes;
        };
        const auto fresh = loopFirstBlock (-1);
        const auto afterHigh = loopFirstBlock (6); // the G pad, high in the register
        const auto afterLow = loopFirstBlock (0);
        check (fresh.size() == 3 && fresh == afterHigh && fresh == afterLow, "the loop starts in the same place however the pads were played before");

        const auto exported = engine::exportNotes (processor.getProgression(), 9, engine::ScaleId::Minor, true, 0);
        std::vector<int> firstChord;
        for (const auto& n : exported)
            if (n.start < 0.01)
                firstChord.push_back (n.note);
        std::sort (firstChord.begin(), firstChord.end());
        check (firstChord == fresh, "and it is the notes of the MIDI file");

        // Suggest again and again: the first chord of the loop never leaves its register by more than the chord itself
        bool steady = true;
        for (int i = 0; i < 40 && steady; ++i)
        {
            processor.setProgression ({});
            processor.suggestProgression();
            const auto one = loopFirstBlock (i % 7);
            const auto two = loopFirstBlock ((i + 3) % 7);
            steady = ! one.empty() && one == two && one.front() >= 36 && one.back() <= 96;
        }
        check (steady, "after Suggest the loop sounds in the same register however the pads were played");

        // The project keeps the register, and the file has it
        *processor.octaveParam = 1;
        juce::MemoryBlock stateBlock;
        processor.getStateInformation (stateBlock);
        ProgrezzorProcessor restored;
        restored.setStateInformation (stateBlock.getData(), (int) stateBlock.getSize());
        check (restored.getGeneralOctave() == 1, "the general register comes back from a saved project");
        *processor.octaveParam = 0;

        // The buttons and the label
        auto& barRef = static_cast<ProgrezzorEditor*> (editor.get())->getBar();
        barRef.getOctaveUpButton().triggerClick();
        settle (50);
        check (processor.getGeneralOctave() == 1 && barRef.getOctaveLabel() == "Oct +1", "Oct+ raises the register and the label says Oct +1");
        barRef.getOctaveDownButton().triggerClick();
        barRef.getOctaveDownButton().triggerClick();
        settle (50);
        check (processor.getGeneralOctave() == -1 && barRef.getOctaveLabel() == "Oct -1", "Oct- lowers it, and the label says Oct -1");
        for (int i = 0; i < 5; ++i)
        {
            barRef.getOctaveDownButton().triggerClick();
            settle (30);
        }
        check (processor.getGeneralOctave() == -2 && ! barRef.getOctaveDownButton().isEnabled(), "it stops at two octaves down");
        // Settings: where the chords usually sit. It is added to the Oct buttons, kept in a file of its own, and the same in every project.
        *processor.octaveParam = 0;
        processor.setHomeOctave (0);
        const auto homeZero = padNotes (0);
        processor.setHomeOctave (1);
        const auto homeOne = padNotes (0);
        *processor.octaveParam = 1;
        const auto homeOnePlusOne = padNotes (0);
        *processor.octaveParam = 0;
        bool homeOk = homeZero.size() == 3 && homeOne.size() == 3 && homeOnePlusOne.size() == 3;
        for (size_t i = 0; homeOk && i < 3; ++i)
            homeOk = homeOne[i] == homeZero[i] + 12 && homeOnePlusOne[i] == homeZero[i] + 24;
        check (homeOk, "Settings: one octave higher moves the pads an octave, and the Oct buttons count on top of it");
        {
            ProgrezzorProcessor another; // another project, later
            check (another.getHomeOctave() == 1, "the setting is kept for every project (it is in a file of its own)");
            another.setHomeOctave (5);
            check (another.getHomeOctave() == 2, "and it stops at two octaves");
        }
        processor.setHomeOctave (-2);
        const auto homeDown = padNotes (0);
        bool downOk = homeDown.size() == 3;
        for (size_t i = 0; downOk && i < 3; ++i)
            downOk = homeDown[i] == homeZero[i] - 24;
        check (downOk, "two octaves lower is 24 semitones down");
        // The file has the same notes as the pads
        {
            processor.setProgression ({ { { 0, engine::ChordType::Minor, engine::kAutoInversion }, 4.0, false } });
            const auto exportedForHome = engine::exportNotes (processor.getProgression(), 9, engine::ScaleId::Minor, true, processor.getTotalOctave());
            check (exportedForHome.size() == 3 && exportedForHome[0].note == homeZero[0] - 24, "the MIDI file follows the setting too");
            processor.setProgression ({});
        }
        processor.setHomeOctave (0);

        // The same pad is always the same notes, whatever was clicked before
        *processor.octaveParam = 0;
        const auto padTwoBefore = padNotes (2);
        padNotes (6);
        padNotes (0);
        padNotes (4);
        check (padNotes (2) == padTwoBefore, "a pad has the same notes after other pads were played (no automatic voice leading)");

        processor.setProgression ({});
        *processor.octaveParam = 0;
        settle (50);
    }

    std::printf ("arp\n");
    {
        struct Event { long long at; int note; bool on; };
        long long blockStart = 0;
        std::vector<Event> events;
        const auto run = [&] (int blocks, juce::AudioPlayHead* head = nullptr)
        {
            processor.setPlayHead (head);
            for (int b = 0; b < blocks; ++b)
            {
                juce::MidiBuffer out;
                juce::AudioBuffer<float> audio (2, 512);
                processor.processBlock (audio, out);
                for (const auto m : out)
                {
                    const auto msg = m.getMessage();
                    if (msg.isNoteOn() || msg.isNoteOff())
                        events.push_back ({ blockStart + m.samplePosition, msg.getNoteNumber(), msg.isNoteOn() });
                }
                blockStart += 512;
            }
            processor.setPlayHead (nullptr);
        };
        const auto onsOf = [&] (const std::vector<Event>& list)
        {
            std::vector<Event> result;
            for (const auto& e : list)
                if (e.on)
                    result.push_back (e);
            return result;
        };

        *processor.keyParam = 9;
        *processor.scaleParam = 0;
        *processor.octaveParam = 0;
        processor.setProgression ({});
        run (4); // what sounds from before ends
        *processor.arpOnParam = true;
        run (2);  // the switch itself
        events.clear();
        blockStart = 0;

        // A pad: the notes of the chord one after the other, a step apart (half a beat at 120 bpm is 12000 samples)
        processor.pressPad (0);
        run (60); // about 0.64 s: steps at 0, 12000 and 24000 samples
        const auto padOns = onsOf (events);
        check (padOns.size() >= 3 && padOns[0].note == 57 && padOns[1].note == 60 && padOns[2].note == 64, "arp on: a pad plays its notes one at a time, up through the chord");
        check (padOns.size() >= 3 && padOns[0].at <= 511 && std::abs (padOns[1].at - padOns[0].at - 12000) <= 1 && std::abs (padOns[2].at - padOns[1].at - 12000) <= 1,
               "a step apart (half a beat), the first at once");
        // Every note ends before the next begins: only one sounds at a time
        bool oneAtATime = true;
        int sounding = 0;
        for (const auto& e : events)
        {
            sounding += e.on ? 1 : -1;
            oneAtATime &= sounding >= 0 && sounding <= 1;
        }
        check (oneAtATime, "only one note sounds at a time");

        // Let go: the note ends, nothing new starts, nothing hangs
        processor.releasePad (0);
        events.clear();
        run (60);
        check (onsOf (events).empty() && processor.soundingNotes() == 0, "letting go of the pad stops the arp, nothing hangs");

        // Rests: hits spread over the steps, and a pad held through the rests ends clean
        *processor.arpHitsParam = 3;
        processor.pressPad (2);
        events.clear();
        run (200);
        const auto restOns = onsOf (events);
        bool spaced = restOns.size() >= 3;
        for (size_t i = 1; spaced && i < 3; ++i)
            spaced = std::abs (restOns[i].at - restOns[i - 1].at - 3 * 12000) <= 1 || std::abs (restOns[i].at - restOns[i - 1].at - 2 * 12000) <= 1 || std::abs (restOns[i].at - restOns[i - 1].at - 3 * 12000) <= 1;
        check (spaced, "with 3 hits in 8 steps the notes are spread (x..x..x.)");
        processor.releasePad (2);
        run (60);
        check (processor.soundingNotes() == 0, "no note hangs after a pad held through the rests is let go");
        *processor.arpHitsParam = 8;

        // Switching Arp off while a pad is held: no note hangs, and the pad is the chord again from the next press
        processor.pressPad (0);
        run (5);
        *processor.arpOnParam = false;
        run (3);
        processor.releasePad (0);
        run (3);
        check (processor.soundingNotes() == 0, "switching Arp off while a pad is held leaves no note hanging");
        events.clear();
        processor.pressPad (0);
        run (2);
        check (onsOf (events).size() == 3, "with the arp off a pad is the whole chord again");
        processor.releasePad (0);
        run (3);

        // The progression: the loop plays the arp, and the MIDI file has the same notes at the same times
        *processor.arpOnParam = true;
        processor.setProgression ({ { { 0, engine::ChordType::Minor, engine::kAutoInversion }, 4.0, false },
                                    { { 8, engine::ChordType::Major, engine::kAutoInversion }, 4.0, false } });
        run (3);
        FakeTransport arpTransport;
        arpTransport.playing = true;
        arpTransport.quarters = 0.0;
        events.clear();
        // About 8 beats (two bars at 120 bpm is 192000 samples = 375 blocks), the transport moving along in step with the blocks
        for (int b = 0; b < 380; ++b)
        {
            arpTransport.quarters = (double) (b * 512) / 24000.0; // 24000 samples is one beat at 120 bpm and 48 kHz
            run (1, &arpTransport);
        }
        std::vector<Event> loopOns = onsOf (events);
        const auto expected = engine::exportNotes (processor.getProgression(), 9, engine::ScaleId::Minor, true, 0, 60, processor.getArpSettings());
        bool matches = loopOns.size() >= expected.size() && expected.size() == 16;
        long long firstAt = loopOns.empty() ? 0 : loopOns.front().at;
        for (size_t i = 0; matches && i < expected.size(); ++i)
            matches = loopOns[i].note == expected[i].note && std::abs ((loopOns[i].at - firstAt) - (long long) std::llround (expected[i].start * 24000.0)) <= 512;
        check (matches, "the loop plays the arp as the MIDI file has it: the same notes, at the same times (to within a block)");
        arpTransport.playing = false;
        run (3, &arpTransport);
        check (processor.soundingNotes() == 0, "when the transport stops no note hangs");

        // Switching Arp off in the middle of the loop leaves nothing hanging either
        arpTransport.playing = true;
        arpTransport.quarters = 0.0;
        run (10, &arpTransport);
        *processor.arpOnParam = false;
        run (3, &arpTransport);
        arpTransport.playing = false;
        run (3, &arpTransport);
        check (processor.soundingNotes() == 0, "switching the arp off while the loop plays leaves no note hanging");

        // The settings are saved with the project
        *processor.arpOnParam = true;
        *processor.arpStyleParam = 3;
        *processor.arpRateParam = 2;
        *processor.arpOctavesParam = 2;
        *processor.arpStepsParam = 12;
        *processor.arpHitsParam = 5;
        *processor.arpRotationParam = 3;
        juce::MemoryBlock arpState;
        processor.getStateInformation (arpState);
        ProgrezzorProcessor arpRestored;
        arpRestored.setStateInformation (arpState.getData(), (int) arpState.getSize());
        const auto restored = arpRestored.getArpSettings();
        check (restored.on && restored.style == 3 && restored.rate == 2 && restored.octaves == 2 && restored.steps == 12 && restored.hits == 5 && restored.rotation == 3,
               "the arp settings come back from a saved project");

        // The button and the panel
        {
            auto& bar = static_cast<ProgrezzorEditor*> (editor.get())->getBar();
            *processor.arpOnParam = false;
            bar.updateButtons();
            check (! bar.getArpButton().getToggleState(), "the Arp button is off when the arp is off");
            *processor.arpOnParam = true;
            bar.updateButtons();
            check (bar.getArpButton().getToggleState(), "and lit when it is on");
            settle (60);
            snapshot ("arp_on");
            *processor.arpOnParam = false;
            bar.updateButtons();
            if (! prefix.empty())
            {
                auto panel = bar.makeArpPanel();
                *processor.arpOnParam = true;
                *processor.arpHitsParam = 5;
                *processor.arpStyleParam = 2;
                *processor.arpRateParam = 2;
                panel->setLookAndFeel (&editor->getLookAndFeel());
                saveSnapshot (*panel, prefix + "_arp_panel.png");
                panel->setLookAndFeel (nullptr);
            }
        }

        *processor.arpOnParam = false;
        *processor.arpStyleParam = 0;
        *processor.arpRateParam = engine::kDefaultArpRate;
        *processor.arpOctavesParam = 1;
        *processor.arpStepsParam = 8;
        *processor.arpHitsParam = 8;
        *processor.arpRotationParam = 0;
        run (3);
        processor.setProgression ({});
    }

    std::printf ("block lengths\n");
    check (BlockButton::lengthText (4.0, 4.0) == "1 bar" && BlockButton::lengthText (2.0, 4.0) == "1/2 bar" && BlockButton::lengthText (8.0, 4.0) == "2 bars"
               && BlockButton::lengthText (6.0, 4.0) == "1 1/2 bars" && BlockButton::lengthText (3.0, 3.0) == "1 bar" && BlockButton::lengthText (32.0, 4.0) == "8 bars",
           "block lengths are written as 1 bar, 1/2 bar, 1 1/2 bars, 2 bars");

    std::printf ("moods and sounds\n");
    {
        // Every mood gives a suggestion in the key and scale, and the moods are not all the same
        *processor.keyParam = 9;
        *processor.scaleParam = 0;
        std::vector<std::string> firsts;
        for (int m = 0; m < engine::kNumMoods; ++m)
        {
            *processor.moodParam = m;
            processor.setProgression ({});
            processor.suggestProgression();
            check (! processor.getProgression().empty(), std::string (engine::moodNames[m]) + " gives a suggestion");
            std::string names;
            for (const auto& b : processor.getProgression())
                names += engine::chordName (b.chord, processor.getKey()) + std::to_string ((int) b.beats) + " ";
            firsts.push_back (names);
        }
        std::sort (firsts.begin(), firsts.end());
        check (std::unique (firsts.begin(), firsts.end()) - firsts.begin() >= 6, "the moods give different progressions");
        *processor.moodParam = (int) engine::Mood::Melancholic;
        processor.setProgression ({});

        // Every sound is heard, stays clean and dies away after the note is let go
        for (int voice = 0; voice < PadSynth::numVoices; ++voice)
        {
            PadSynth synth;
            synth.prepare (48000.0);
            synth.setVoice (voice);
            std::vector<float> left (512), right (512);
            synth.noteOn (57, 100);
            synth.noteOn (60, 100);
            synth.noteOn (64, 100);
            double energy = 0.0;
            bool clean = true;
            for (int b = 0; b < 80; ++b)
            {
                std::fill (left.begin(), left.end(), 0.0f);
                std::fill (right.begin(), right.end(), 0.0f);
                synth.render (left.data(), right.data(), 0, 512);
                for (int i = 0; i < 512; ++i)
                {
                    energy += left[(size_t) i] * left[(size_t) i];
                    clean &= std::isfinite (left[(size_t) i]) && std::isfinite (right[(size_t) i]) && std::abs (left[(size_t) i]) < 1.5f;
                }
            }
            check (energy > 0.01 && clean, "sound " + std::to_string (voice) + " is heard and clean");
            synth.noteOff (57);
            synth.noteOff (60);
            synth.noteOff (64);
            for (int b = 0; b < 400; ++b)
            {
                std::fill (left.begin(), left.end(), 0.0f);
                std::fill (right.begin(), right.end(), 0.0f);
                synth.render (left.data(), right.data(), 0, 512);
            }
            check (synth.isSilent(), "sound " + std::to_string (voice) + " dies away after the notes are let go");
        }
    }

    {
        // Off: no preview sound at all, but the notes are still sent
        PadSynth synth;
        synth.prepare (48000.0);
        synth.setVoice (PadSynth::Off);
        synth.noteOn (60, 100);
        std::vector<float> left (512), right (512);
        synth.render (left.data(), right.data(), 0, 512);
        double energy = 0.0;
        for (const float v : left)
            energy += v * v;
        check (energy < 1.0e-12 && synth.isSilent(), "sound Off makes no preview sound");

        *processor.voiceParam = PadSynth::Off;
        juce::MidiBuffer midiOut;
        juce::AudioBuffer<float> audio (2, 512);
        for (int i = 0; i < 4; ++i) // what sounded before fades away
            processor.processBlock (audio, midiOut);
        processor.pressPad (0);
        processor.processBlock (audio, midiOut);
        bool noteOn = false;
        for (const auto m : midiOut)
            noteOn |= m.getMessage().isNoteOn();
        check (noteOn && audio.getMagnitude (0, 512) < 1.0e-9f, "with sound Off the MIDI is still sent and the audio is silent");
        processor.releasePad (0);
        processor.processBlock (audio, midiOut);
        *processor.voiceParam = 0;
    }

    std::printf ("state\n");
    *processor.keyParam = 2;
    *processor.scaleParam = 3;
    *processor.loopParam = false;
    *processor.lockParam = false;
    juce::MemoryBlock saved;
    processor.getStateInformation (saved);
    ProgrezzorProcessor other;
    other.setStateInformation (saved.getData(), (int) saved.getSize());
    check (other.keyParam->getIndex() == 2 && other.scaleParam->getIndex() == 3, "key and scale come back from a saved project");
    check (! other.loopParam->get(), "and so does Play");
    check (! other.lockParam->get(), "and Lock to scale");
    *processor.moodParam = (int) engine::Mood::Driving;
    *processor.voiceParam = 4;
    juce::MemoryBlock savedMood;
    processor.getStateInformation (savedMood);
    ProgrezzorProcessor moodOther;
    moodOther.setStateInformation (savedMood.getData(), (int) savedMood.getSize());
    check (moodOther.getMood() == engine::Mood::Driving && moodOther.voiceParam->getIndex() == 4, "mood and sound come back from a saved project");
    *processor.moodParam = (int) engine::Mood::Melancholic;
    *processor.voiceParam = 0;
    bool sameProgression = other.getProgression().size() == processor.getProgression().size();
    for (size_t i = 0; sameProgression && i < processor.getProgression().size(); ++i)
        sameProgression = other.getProgression()[i].chord == processor.getProgression()[i].chord
                          && std::abs (other.getProgression()[i].beats - processor.getProgression()[i].beats) < 1.0e-9;
    check (sameProgression, "the progression comes back from a saved project");

    editor.reset();
    std::printf (failures == 0 ? "ALL OK\n" : "%d FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
