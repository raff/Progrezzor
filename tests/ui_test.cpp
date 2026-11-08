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
    check (processor.isPlayOn(), "Play lights by itself while the DAW plays");
    processor.togglePlay(); // pressing it does nothing: the DAW is the one that stops
    runBlock (processor);
    check (processor.isPlayOn() && processor.getPlayingBlock() == 1 && processor.soundingNotes() == 3, "and it cannot be switched off");
    transport.playing = false;
    runBlock (processor);

    // Play with the DAW stopped: the plug-in's own clock plays the progression from its start
    transport.quarters = 0.0;
    check (! processor.isPlayOn(), "Play is off while the DAW is stopped");
    processor.togglePlay();
    runBlock (processor);
    check (processor.isPlayOn() && processor.getPlayingBlock() == 0 && processor.soundingNotes() == 3, "Play plays the first chord on its own");
    {
        // A pad or a block pressed while Play is lit gives no sound and does not silence the loop
        processor.pressPad (3);
        processor.pressBlock (5, { 7, engine::ChordType::Major, engine::kAutoInversion });
        juce::AudioBuffer<float> audio (2, 512);
        juce::MidiBuffer out;
        processor.processBlock (audio, out);
        check (out.getNumEvents() == 0 && processor.soundingNotes() == 3, "pads and blocks are silent while Play is lit, the loop goes on");
        processor.releasePad (3);
        processor.releaseBlock (5);
        runBlock (processor);
    }
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

    // A host that says it plays but never moves its position (seen in FL Studio): the loop must still go on to the next chord
    transport.playing = true;
    transport.quarters = 0.0;
    runBlock (processor);
    check (processor.getPlayingBlock() == 0, "a host that stands still starts with the first chord");
    for (int i = 0; i < 200; ++i) // about four beats at 120 bpm, the host's position never changes
        runBlock (processor);
    check (processor.getPlayingBlock() == 1, "and the loop still goes on to the next chord");
    transport.playing = false;
    runBlock (processor);
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
        check (roots() == "0 4 7 ", "Delete removes the block");
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
            {
                const auto tray = bar.getTrayBounds();
                const auto button = bar.getReplaceButton().getBounds();
                check (std::abs (button.getCentreY() - tray.getCentreY()) <= 4 && button.getCentreX() > tray.getCentreX() && button.getCentreX() < tray.getRight(),
                       "the question is in the middle of the bar (the Replace button is on the right of the middle)");
            }
            snapshot ("suggest_asks");
            check (bar.getCancelReplaceButton().isVisible() && bar.getCancelReplaceButton().getRight() <= bar.getReplaceButton().getX(), "there is a Cancel button next to Replace");
            bar.getCancelReplaceButton().triggerClick();
            settle (50);
            check (processor.getProgression() == mine && ! bar.getReplaceButton().isVisible() && ! bar.getCancelReplaceButton().isVisible(),
                   "Cancel leaves the chords as they are and closes the question");
            ui.getSuggestButton().triggerClick();
            settle (50);
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

        // Rec is a switch: while it is on, every chord played goes at the end of the bar as one bar
        {
            const auto barBefore = processor.getProgression();
            check (! bar.isRecOn(), "Rec starts off");
            processor.pressPad (4);
            runBlock (processor);
            processor.releasePad (4);
            runBlock (processor);
            bar.updateButtons();
            check (processor.getProgression() == barBefore, "a chord played while Rec is off adds nothing");

            bar.getRecButton().triggerClick();
            settle (50);
            check (bar.isRecOn() && bar.getRecButton().getToggleState(), "the Rec button switches it on");
            snapshot ("rec_on");
            for (int pad : { 1, 3, 3 })
            {
                processor.pressPad (pad);
                runBlock (processor);
                processor.releasePad (pad);
                runBlock (processor);
                bar.updateButtons();
            }
            const auto now = processor.getProgression();
            check (now.size() == barBefore.size() + 3, "each chord played is added");
            check (now[barBefore.size()].chord == processor.getPadChord (1) && now[barBefore.size() + 2].chord == processor.getPadChord (3)
                       && near (now.back().beats, processor.getHostBarBeats()),
                   "at the end, in the order played, one bar each");
            check (std::vector<engine::Block> (now.begin(), now.begin() + (long) barBefore.size()) == barBefore, "the chords that were there are not changed");

            processor.undo();
            check (processor.getProgression().size() == now.size() - 1, "Undo takes the last one away");
            processor.redo();

            // A pad pulled into the bar while Rec is on counts once (the press that starts the drag is not added too)
            {
                const auto sizeBefore = processor.getProgression().size();
                processor.pressPad (2);
                runBlock (processor);
                bar.updateButtons();
                ui.getPad (2).onDragStart();
                processor.releasePad (2);
                runBlock (processor);
                ui.getPad (2).onDragEnd (bar.localPointToGlobal (juce::Point<int> (bar.getBlock (0)->getBounds().getRight() + 2, bar.getBlock (0)->getBounds().getCentreY())));
                settle (50);
                bar.updateButtons();
                check (processor.getProgression().size() == sizeBefore + 1, "a pad dragged in while Rec is on is added once");
                check (processor.getProgression()[1].chord == processor.getPadChord (2), "and it is where it was let go");
                processor.undo(); // the drop
                settle (50);
            }

            bar.getRecButton().triggerClick();
            settle (50);
            check (! bar.isRecOn(), "the Rec button switches it off");
            processor.pressPad (5);
            runBlock (processor);
            processor.releasePad (5);
            runBlock (processor);
            bar.updateButtons();
            check (processor.getProgression() == now, "and then nothing more is added");

            bar.getRecButton().triggerClick();
            settle (50);
            check (bar.isRecOn(), "Rec on again");
            processor.togglePlay();
            settle (80);
            bar.updateButtons();
            check (! bar.isRecOn() && ! bar.getRecButton().isEnabled(), "Play switches Rec off, and it cannot be on while playing");
            processor.togglePlay();
            settle (80);
            processor.setProgression (barBefore);
            settle (50);
        }

        // The blank pad: a pause
        {
            const auto barBefore = processor.getProgression();
            const auto screenAt = [&] (int block, bool rightEdge)
            {
                const auto b = bar.getBlock (block)->getBounds();
                return bar.localPointToGlobal (juce::Point<int> (rightEdge ? b.getRight() + 2 : b.getCentreX(), b.getCentreY()));
            };
            auto& restPad = ui.getRestPad();
            check (restPad.getWidth() > 0 && restPad.getWidth() < ui.getPad (0).getWidth(), "the blank pad is narrower than a chord pad");

            // A click on it does nothing (and has no sound)
            restPad.onPress();
            runBlock (processor);
            check (processor.getProgression() == barBefore && processor.soundingNotes() == 0, "a click on the blank pad adds nothing and makes no sound");

            // Pulled between blocks it adds a pause of one bar
            restPad.onDragStart();
            restPad.onDragEnd (screenAt (0, true));
            settle (50);
            check (processor.getProgression().size() == barBefore.size() + 1 && processor.getProgression()[1].rest
                       && near (processor.getProgression()[1].beats, processor.getHostBarBeats()),
                   "dragged between blocks it adds a pause of one bar");
            check (! processor.getProgression()[0].rest && processor.getProgression()[2] == barBefore[1], "the other blocks stay as they were");
            snapshot ("rest_in_bar");
            {
                // The sign follows the length: half rest, whole rest, both, and the multi-bar rest with its number
                const auto shown = processor.getProgression();
                engine::Progression lengths;
                for (double beats : { 2.0, 4.0, 6.0, 8.0, 12.0 })
                {
                    engine::Block block;
                    block.rest = true;
                    block.beats = beats;
                    lengths.push_back (block);
                }
                processor.setProgression (lengths);
                settle (100);
                snapshot ("rest_lengths");
                processor.setProgression (shown);
                settle (50);
            }

            // Pulled onto the middle of a block it turns that block into a pause (the length is kept)
            const auto lengthOfThird = processor.getProgression()[2].beats;
            restPad.onDragStart();
            restPad.onDragEnd (screenAt (2, false));
            settle (50);
            check (processor.getProgression()[2].rest && near (processor.getProgression()[2].beats, lengthOfThird) && processor.getProgression().size() == barBefore.size() + 1,
                   "dragged onto a block it replaces that block with a pause");

            // A chord on a pause makes it a chord again
            ui.getPad (3).onDragStart();
            ui.getPad (3).onDragEnd (screenAt (2, false));
            settle (50);
            check (! processor.getProgression()[2].rest && processor.getProgression()[2].chord == processor.getPadChord (3), "a chord dragged onto a pause makes it a chord");

            // The pause does not sound in Play, and it is quiet in the loop
            processor.undo();
            processor.undo();
            processor.undo();
            settle (50);
            check (processor.getProgression() == barBefore, "undo brings the bar back after the pauses");
            processor.editProgression (engine::withRestAt (processor.getProgression(), 0));
            settle (50);
            bar.getBlock (0)->onPress();
            runBlock (processor);
            check (processor.soundingNotes() == 0, "pressing a pause plays nothing");
            {
                bar.setEditing (false); // Play: the loop sounds
                FakeTransport t3;
                processor.setPlayHead (&t3);
                t3.playing = true;
                t3.quarters = 1.0; // inside the first block, the pause
                runBlock (processor);
                check (processor.getPlayingBlock() == 0 && processor.soundingNotes() == 0, "the loop is quiet during a pause");
                t3.quarters = 5.0;
                runBlock (processor);
                check (processor.soundingNotes() > 0, "and plays again after it");
                t3.playing = false;
                runBlock (processor);
                processor.setPlayHead (nullptr);
                bar.setEditing (true);
            }

            // It is saved with the project
            juce::MemoryBlock saved;
            processor.getStateInformation (saved);
            ProgrezzorProcessor other;
            other.setStateInformation (saved.getData(), (int) saved.getSize());
            check (other.getProgression() == processor.getProgression() && other.getProgression()[0].rest, "a pause is saved and comes back with the project");

            // With Rec on, a click on the blank pad adds a pause at the end, and a drag into the bar adds it once
            processor.setProgression (barBefore);
            settle (50);
            bar.getRecButton().triggerClick();
            settle (50);
            check (bar.isRecOn(), "Rec is on");
            restPad.onPress();
            settle (50);
            check (processor.getProgression().size() == barBefore.size() + 1 && processor.getProgression().back().rest, "with Rec on a click adds a pause at the end");
            restPad.onPress();
            restPad.onDragStart();
            restPad.onDragEnd (screenAt (0, true));
            settle (50);
            check (processor.getProgression().size() == barBefore.size() + 2 && processor.getProgression()[1].rest, "a pause dragged in with Rec on is added once, where it lands");
            bar.getRecButton().triggerClick();
            settle (50);
            processor.setProgression (barBefore);
            settle (50);
        }

        // A new project starts in C Major with the plainest mood
        {
            ProgrezzorProcessor fresh;
            check (fresh.getKey().tonic == 0 && fresh.getKey().scale == engine::ScaleId::Major && fresh.getMood() == engine::Mood::Bright,
                   "a new project starts in C Major, in the Bright mood");
        }

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

        // The DAW plays the loop, and Play follows it
        FakeTransport t3;
        processor.setPlayHead (&t3);
        processor.setProgression ({ mk (0, 4), mk (2, 4) });
        t3.playing = true;
        t3.quarters = 1.0;
        runBlock (processor);
        check (processor.getPlayingBlock() == 0 && processor.soundingNotes() == 3 && processor.isPlayOn(), "the DAW plays the loop and Play is lit");
        t3.playing = false;
        runBlock (processor);
        check (! processor.isPlayOn(), "and when the DAW stops, Play is off again (the bar can be edited)");
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

        check (bar.getQuickTextButton().isVisible() && ! bar.getQuickTextButton().isEnabled(), "the pencil is greyed in Play");
        bar.setEditing (true);
        check (bar.getQuickTextButton().isVisible() && bar.getQuickTextButton().isEnabled(), "the pencil is enabled in Edit");
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

        // A chord chart with bar lines, pasted over several lines
        click (bar.getQuickTextButton());
        bar.getTextEditor().selectAll();
        bar.getTextEditor().insertTextAtCaret ("| Am | F |\n| C G | %\n|");
        bar.applyQuickText();
        settle (50);
        {
            const auto& q = processor.getProgression();
            check (! bar.isInTextMode() && q.size() == 5 && near (q[0].beats, 4.0) && near (q[2].beats, 2.0) && near (q[3].beats, 2.0) && near (q[4].beats, 4.0) && q[4].chord == q[3].chord,
                   "a chord chart with bars and % is read, over several lines");
        }

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

        // Suggest closes the text
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

    std::printf ("learn mode\n");
    {
        auto& learnUi = *static_cast<ProgrezzorEditor*> (editor.get());
        auto& learnBar = learnUi.getBar();
        *processor.keyParam = 0;
        *processor.scaleParam = 1;
        *processor.arpOnParam = false;
        processor.setLearnHints (false);

        // "Inversions in suggestions": on as standard, and kept for every project
        check (processor.getSuggestInversions(), "inversions in suggestions are on as standard");
        processor.setSuggestInversions (false);
        {
            ProgrezzorProcessor another;
            check (! another.getSuggestInversions(), "the choice about inversions in suggestions is kept for every project");
        }
        processor.setSuggestInversions (true);
        processor.setProgression ({ { { 0, engine::ChordType::Major, engine::kAutoInversion }, 4.0, false },   // C
                                    { { 5, engine::ChordType::Major, engine::kAutoInversion }, 4.0, false },   // F
                                    { { 9, engine::ChordType::Minor, engine::kAutoInversion }, 4.0, false } }); // Am
        settle (80);
        check (! processor.isLearning() && ! learnBar.getLearnButton().getToggleState(), "Learn is off to begin with");

        {
            // No key from an earlier test is still down
            juce::MidiBuffer clear;
            clear.addEvent (juce::MidiMessage::allNotesOff (1), 0);
            juce::AudioBuffer<float> audio (2, 512);
            processor.processBlock (audio, clear);
        }
        learnBar.getLearnButton().triggerClick();
        settle (120);
        check (processor.isLearning() && learnBar.getLearnButton().getToggleState(), "the Learn button switches Learn mode on");
        check (learnBar.getLearnTarget() == 0, "the first chord is the target");

        // Keys of a MIDI keyboard: the notes in a block of audio, with the keys held down in later blocks
        std::vector<int> heard;
        const auto pressKeys = [&] (std::initializer_list<int> down, std::initializer_list<int> up, int wait)
        {
            juce::MidiBuffer in;
            for (int n : up)
                in.addEvent (juce::MidiMessage::noteOff (1, n), 0);
            for (int n : down)
                in.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 90), 0);
            juce::MidiBuffer out;
            juce::AudioBuffer<float> audio (2, 512);
            processor.processBlock (audio, in);
            (void) out;
            settle (wait);
        };
        const auto soundingKeys = [&] { return processor.soundingNotes(); };

        // A pad click is not a chord in Learn mode
        processor.pressPad (0);
        runBlock (processor);
        processor.releasePad (0);
        runBlock (processor);
        settle (120);
        check (learnBar.getLearnTarget() == 0, "clicking the pad does not count: the keys do");

        // The right keys held at the same time: C E G
        pressKeys ({ 60, 64 }, {}, 60);
        check (learnBar.getLearnTarget() == 0, "two of the three keys is not the chord yet, and not wrong either");
        pressKeys ({ 67 }, {}, 120);
        check (learnBar.getLearnTarget() == 1, "the three keys of C move on to the next chord");
        check (soundingKeys() == 3, "the keys play their own notes");
        pressKeys ({}, { 60, 64, 67 }, 80);
        check (soundingKeys() == 0, "and let go of them again");

        // F in another octave and order, with a doubled note: right
        pressKeys ({ 69, 53, 60, 65 }, {}, 120);
        check (learnBar.getLearnTarget() == 2, "F in another octave and order is right");
        pressKeys ({}, { 69, 53, 60, 65 }, 80);

        // The wrong chord, held: red, and after two wrong tries the hint
        pressKeys ({ 60, 64, 67 }, {}, 700);
        check (learnBar.getLearnTarget() == 2, "the wrong chord does not move on");
        snapshot ("learn_wrong");
        pressKeys ({}, { 60, 64, 67 }, 120);
        pressKeys ({ 62, 65 }, {}, 60);
        pressKeys ({}, { 62, 65 }, 120);
        settle (60);
        snapshot ("learn_hint");
        pressKeys ({ 57, 60, 64 }, {}, 120); // Am
        settle (120);
        check (learnBar.getLearnTarget() == -1, "the last chord finishes it");
        snapshot ("learn_done");
        pressKeys ({}, { 57, 60, 64 }, 80);
        check (soundingKeys() == 0, "nothing hangs");

        // The loop is silent in Learn mode, and so is the arp
        FakeTransport learnTransport;
        processor.setPlayHead (&learnTransport);
        learnTransport.playing = true;
        learnTransport.quarters = 0.0;
        runBlock (processor);
        runBlock (processor);
        check (processor.getPlayingBlock() == -1 && processor.soundingNotes() == 0, "the loop does not play in Learn mode");
        learnTransport.playing = false;
        runBlock (processor);
        processor.setPlayHead (nullptr);

        // Hints from the settings are kept for every project
        processor.setLearnHints (true);
        {
            ProgrezzorProcessor another;
            check (another.getLearnHints(), "the choice about hints is kept for every project");
        }
        processor.setLearnHints (false);

        // Switching it off again: the keys are the pads again
        learnBar.getLearnButton().triggerClick();
        settle (120);
        check (! processor.isLearning(), "the Learn button switches it off again");
        pressKeys ({ 60 }, {}, 40);
        check (processor.litPads() != 0, "with Learn off a key plays a pad again");
        pressKeys ({}, { 60 }, 40);
        processor.setProgression ({});
        settle (60);
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

    {
        std::printf ("block menu\n");
        auto& bar = static_cast<ProgrezzorEditor*> (editor.get())->getBar();
        bar.setEditing (true);
        const auto mk = [] (int root, engine::ChordType type, double beats) { engine::Block b; b.chord = { root, type, engine::kAutoInversion }; b.beats = beats; return b; };
        const int oldKey = processor.keyParam->getIndex(), oldScale = processor.scaleParam->getIndex();
        *processor.keyParam = 0;
        *processor.scaleParam = (int) engine::ScaleId::Major;
        const auto padBefore = processor.getPadChord (4); // the G pad
        processor.setProgression ({ mk (0, engine::ChordType::Major, 4), mk (7, engine::ChordType::Major, 4), mk (9, engine::ChordType::Minor, 4) });
        settle (60);
        const auto original = processor.getProgression();

        // Right-click does not delete any more: it opens the menu (a mouse-down with the right button changes nothing by itself)
        const auto rightClick = [&] (juce::Component& c)
        {
            return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), { 40.0f, 30.0f }, juce::ModifierKeys (juce::ModifierKeys::rightButtonModifier),
                                     0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c, juce::Time::getCurrentTime(), { 40.0f, 30.0f }, juce::Time::getCurrentTime(), 1, false);
        };
        bool menuAsked = false;
        bar.getBlock (1)->onMenu = [&] { menuAsked = true; };
        bar.getBlock (1)->mouseDown (rightClick (*bar.getBlock (1)));
        bar.getBlock (1)->mouseUp (rightClick (*bar.getBlock (1)));
        check (menuAsked && processor.getProgression() == original, "right-click asks for the menu and deletes nothing");
        bar.refresh();
        settle (60);

        bar.getBlock (1)->setForceHover (true);
        settle (60);
        snapshot ("block_gear");
        bar.getBlock (1)->setForceHover (false);

        // Type: G becomes G7, in this block only
        bar.applyBlockMenuChoice (1, 100 + (int) engine::ChordType::Dom7);
        check (processor.getProgression()[1].chord.type == engine::ChordType::Dom7 && processor.getProgression()[0] == original[0], "the type changes in the block only");
        check (processor.getPadChord (4) == padBefore, "the pad does not change");
        // Inversion
        bar.applyBlockMenuChoice (1, 201);
        check (processor.getProgression()[1].chord.inversion == 1, "the inversion changes in the block");
        // Octave
        bar.applyBlockMenuChoice (1, 301);
        check (processor.getProgression()[1].chord.octave == 1, "octave up");
        bar.applyBlockMenuChoice (1, 302);
        bar.applyBlockMenuChoice (1, 302);
        check (processor.getProgression()[1].chord.octave == -1, "octave down");
        // Length: 1/2 bar, 2 bars
        bar.applyBlockMenuChoice (1, 310);
        check (std::abs (processor.getProgression()[1].beats - 0.5 * processor.getHostBarBeats()) < 1.0e-9, "the length can be half a bar");
        bar.applyBlockMenuChoice (1, 312);
        check (std::abs (processor.getProgression()[1].beats - 2.0 * processor.getHostBarBeats()) < 1.0e-9, "and 2 bars");

        // What the block has is what the MIDI file has
        const auto edited = processor.getProgression();
        const auto notes = engine::exportNotes (edited, 0, engine::ScaleId::Major);
        const auto voiced = engine::shiftedByOctaves (engine::voiceChord (edited[1].chord, 0), 0);
        bool inFile = false;
        for (const auto& n : notes)
            inFile |= std::abs (n.start - edited[0].beats) < 1.0e-9 && std::abs (n.length - edited[1].beats) < 1.0e-9 && n.note == voiced.notes[0];
        check (inFile && voiced.count == 4 && edited[1].chord.type == engine::ChordType::Dom7, "the MIDI file has the block's chord: type, inversion, octave and length");

        // "More chords outside the key...": the lock goes off, and then the menus offer every chord
        {
            ProgrezzorProcessor fresh;
            check (fresh.isLockedToScale(), "the lock is on as standard");
        }
        *processor.lockParam = true;
        processor.turnLockOff();
        check (! processor.isLockedToScale(), "More chords outside the key... turns the lock off");
        *processor.lockParam = true;

        // Lock, Duplicate, Delete
        bar.applyBlockMenuChoice (1, 320);
        check (processor.getProgression()[1].locked, "Lock locks the block");
        bar.applyBlockMenuChoice (1, 321);
        check (processor.getProgression().size() == 4 && processor.getProgression()[2].chord == processor.getProgression()[1].chord && ! processor.getProgression()[2].locked,
               "Duplicate puts a copy after the block");
        bar.applyBlockMenuChoice (2, 330);
        check (processor.getProgression().size() == 3, "Delete removes the block");

        // Undo takes every change back, one at a time
        for (int i = 0; i < 10; ++i) // the ten changes above
            processor.undo();
        check (processor.getProgression() == original, "Undo takes all of it back");
        check (processor.getPadChord (4) == padBefore, "the pad is the same at the end");

        // A pause: only a length, Duplicate and Delete (the menu does not offer the rest; a type choice does nothing)
        processor.setProgression ({ mk (0, engine::ChordType::Major, 4) });
        bar.applyBlockMenuChoice (0, 100 + (int) engine::ChordType::Minor);
        processor.setProgression (engine::withRestAt (processor.getProgression(), 0));
        bar.applyBlockMenuChoice (0, 100 + (int) engine::ChordType::Minor);
        bar.applyBlockMenuChoice (0, 301);
        check (processor.getProgression()[0].rest, "a pause stays a pause");
        bar.applyBlockMenuChoice (0, 311);
        check (processor.getProgression()[0].rest && std::abs (processor.getProgression()[0].beats - processor.getHostBarBeats()) < 1.0e-9, "a pause can change its length");

        *processor.keyParam = oldKey;
        *processor.scaleParam = oldScale;
        settle (60);
    }

    {
        std::printf ("chord wheel\n");
        auto& ui = *static_cast<ProgrezzorEditor*> (editor.get());
        const int oldKey = processor.keyParam->getIndex(), oldScale = processor.scaleParam->getIndex();
        *processor.keyParam = 0;
        *processor.scaleParam = (int) engine::ScaleId::Major;
        settle (60);

        auto& wheelButton = ui.getWheelButton();
        check (! ui.isWheelOpen(), "the wheel is closed at the start");
        wheelButton.triggerClick();
        settle (60);
        check (ui.isWheelOpen(), "the wheel button opens the wheel");
        snapshot ("wheel_c_major");

        juce::Component* wheel = nullptr;
        if (auto* panel = ui.getWheelPanel())
            wheel = panel->getChildComponent (0);
        check (wheel != nullptr && wheel->getWidth() == 380, "the wheel is a popup of its own");

        if (wheel != nullptr)
        {
            const auto mouse = [&] (juce::Point<float> where, int clicks)
            {
                return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), where, juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier),
                                         0.0f, 0.0f, 0.0f, 0.0f, 0.0f, wheel, wheel, juce::Time::getCurrentTime(), where, juce::Time::getCurrentTime(), clicks, false);
            };
            const auto at = [] (float degrees, float radius)
            {
                const float a = juce::degreesToRadians (degrees);
                return juce::Point<float> (190.0f + std::sin (a) * radius, 190.0f - std::cos (a) * radius);
            };

            juce::MidiBuffer allOff;
            allOff.addEvent (juce::MidiMessage::allNotesOff (1), 0);
            runBlock (processor, allOff);
            runBlock (processor);
            check (processor.soundingNotes() == 0, "quiet before the wheel is tried");

            // Straight up on the outer ring is C: a click plays C major (C E G)
            wheel->mouseDown (mouse (at (0.0f, 150.0f), 1));
            runBlock (processor);
            check (processor.soundingNotes() == 3, "a click on a chord plays it");
            wheel->mouseUp (mouse (at (0.0f, 150.0f), 1));
            runBlock (processor);
            check (processor.soundingNotes() == 0, "and it stops when the mouse is let go");
            check (processor.keyParam->getIndex() == 0, "one click does not change the key");

            // Inner ring, 90 degrees round (three steps on the circle of fifths): Em, which is a pad in C major
            wheel->mouseDown (mouse (at (90.0f, 96.0f), 1));
            runBlock (processor);
            check (processor.soundingNotes() == 3, "an inner chord plays too");
            wheel->mouseUp (mouse (at (90.0f, 96.0f), 1));
            runBlock (processor);

            // Outer ring, 90 degrees round: A major, not in C major. It still plays when it is clicked.
            wheel->mouseDown (mouse (at (90.0f, 150.0f), 1));
            runBlock (processor);
            check (processor.soundingNotes() == 3, "a chord outside the scale plays when it is clicked");
            wheel->mouseUp (mouse (at (90.0f, 150.0f), 1));
            runBlock (processor);

            // The middle: Bdim, the chord of its own
            wheel->mouseDown (mouse (at (0.0f, 0.0f), 1));
            runBlock (processor);
            check (processor.soundingNotes() == 3, "the chord in the middle plays");
            wheel->mouseUp (mouse (at (0.0f, 0.0f), 1));
            runBlock (processor);

            // While Play is on a double-click changes nothing
            processor.togglePlay();
            runBlock (processor);
            check (processor.isPlayOn(), "Play is on");
            wheel->mouseDoubleClick (mouse (at (90.0f, 150.0f), 2));
            settle (60);
            check (processor.keyParam->getIndex() == 0, "a double-click on the wheel does not change the key while Play is on");
            processor.togglePlay();
            runBlock (processor);

            // A double-click on A major makes the key A, and it is still major
            wheel->mouseDoubleClick (mouse (at (90.0f, 150.0f), 2));
            settle (60);
            check (processor.keyParam->getIndex() == 9 && processor.getKey().scale == engine::ScaleId::Major, "a double-click on A major makes the key A major");
            snapshot ("wheel_a_major");

            // And on an inner chord of a major key: the minor key with that chord (F#m: 60 degrees round, inner)
            wheel->mouseDoubleClick (mouse (at (120.0f, 96.0f), 2));
            settle (60);
            check (processor.getKey().scale == engine::ScaleId::Minor, "a double-click on a minor chord in a major key makes the key minor");
            snapshot ("wheel_minor");
        }

        // It stays open when the window loses the focus, and Esc, a click outside, or the button closes it
        check (ui.isWheelOpen(), "the wheel is still open after everything above");
        ui.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
        check (! ui.isWheelOpen(), "Esc closes the wheel");
        wheelButton.triggerClick();
        settle (60);
        check (ui.isWheelOpen(), "the button opens it again");
        wheelButton.triggerClick();
        settle (60);
        check (! ui.isWheelOpen(), "the wheel button closes it again");
        wheelButton.triggerClick();
        settle (60);
        ui.mouseDown (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), { 5.0f, 5.0f }, juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier),
                                        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &ui.getPad (0), &ui.getPad (0), juce::Time::getCurrentTime(), { 5.0f, 5.0f }, juce::Time::getCurrentTime(), 1, false));
        check (! ui.isWheelOpen(), "a click on something else in the window closes it");

        *processor.keyParam = oldKey;
        *processor.scaleParam = oldScale;
        settle (60);
    }

    {
        std::printf ("chord rhythm panel\n");
        auto& bar = static_cast<ProgrezzorEditor*> (editor.get())->getBar();
        auto panel = bar.makeArpPanel();
        panel->setLookAndFeel (&editor->getLookAndFeel());
        const auto click = [&] (juce::Point<int> where)
        {
            const juce::Point<float> p ((float) where.x, (float) where.y);
            panel->mouseDown (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), p, juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier),
                                                0.0f, 0.0f, 0.0f, 0.0f, 0.0f, panel.get(), panel.get(), juce::Time::getCurrentTime(), p, juce::Time::getCurrentTime(), 1, false));
            panel->mouseUp (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), p, juce::ModifierKeys(),
                                              0.0f, 0.0f, 0.0f, 0.0f, 0.0f, panel.get(), panel.get(), juce::Time::getCurrentTime(), p, juce::Time::getCurrentTime(), 1, false));
        };
        const auto shot = [&] (const char* name) { if (! prefix.empty()) saveSnapshot (*panel, prefix + "_" + name + ".png"); };

        *processor.arpOnParam = false;
        *processor.rhythmModeParam = false;
        shot ("rhythm_hold");
        click ({ 16 + 114 * 2 + 20, 20 }); // Rhythm
        check (processor.arpOnParam->get() && processor.rhythmModeParam->get() && processor.getRhythmSettings().on && ! processor.getArpSettings().on, "the switch: Rhythm");
        shot ("rhythm_panel");
        click ({ 16 + 86 * 2 + 20, 66 + 56 + 20 });   // third tile in the second row: pattern 6, Gallop
        check (processor.rhythmPatternParam->getIndex() == 6, "a pattern is chosen from the tiles");
        click ({ 16 + 68 + 20, 270 });                // 1/16
        check (processor.rhythmSpeedParam->getIndex() == 1, "Speed 1/16");
        click ({ 156 + 68 * 2 + 20, 270 });           // Long
        check (processor.rhythmLengthParam->getIndex() == 2, "Length Long");
        shot ("rhythm_gallop");
        click ({ 16 + 86 * 3 + 20, 66 + 112 + 20 });  // Euclid
        check (processor.rhythmPatternParam->getIndex() == 11, "Euclid");
        click ({ 16 + 150, 302 });                    // Hits +
        click ({ 190 + 150, 302 });                   // Turn +
        check (processor.arpHitsParam->get() == 9 && processor.arpRotationParam->get() == 1, "Euclid has Hits and Turn");
        shot ("rhythm_euclid");
        click ({ 16 + 114 + 20, 20 });                // Arp
        check (processor.getArpSettings().on && ! processor.getRhythmSettings().on, "the switch: Arp");
        shot ("rhythm_arp");
        click ({ 16 + 20, 20 });                      // Hold
        check (! processor.arpOnParam->get() && ! processor.getArpSettings().on && ! processor.getRhythmSettings().on, "the switch: Hold");

        // Everything is saved with the project
        *processor.rhythmModeParam = true;
        *processor.arpOnParam = true;
        juce::MemoryBlock savedRhythm;
        processor.getStateInformation (savedRhythm);
        ProgrezzorProcessor restored;
        restored.setStateInformation (savedRhythm.getData(), (int) savedRhythm.getSize());
        check (restored.getRhythmSettings().on && restored.getRhythmSettings().pattern == 11 && restored.getRhythmSettings().speed == 1
               && restored.getRhythmSettings().length == 2 && restored.getRhythmSettings().hits == 9 && restored.getRhythmSettings().rotation == 1,
               "the chord rhythm settings are saved with the project");

        // A pad plays the whole chord in the rhythm: sound only at the strikes (Euclid 9 of 16 here, strike at slot 1 after Turn)
        panel->setLookAndFeel (nullptr);
        *processor.arpOnParam = false;
        *processor.rhythmModeParam = false;
        *processor.rhythmPatternParam = 0;
        *processor.rhythmSpeedParam = 0;
        *processor.rhythmLengthParam = 1;
        *processor.arpHitsParam = 8;
        *processor.arpRotationParam = 0;
    }

    {
        std::printf ("preview sound\n");
        auto& ui = *static_cast<ProgrezzorEditor*> (editor.get());
        check (processor.voiceParam->getIndex() == 0 && processor.voiceParam->getCurrentChoiceName() == "Soft Pad", "preview is on as standard, with Soft Pad");
        settle (60);
        snapshot ("sound_on");

        // A project saved with the old Pad (number 0) still has the same sound
        juce::MemoryBlock oldProject;
        processor.getStateInformation (oldProject);
        ProgrezzorProcessor loaded;
        loaded.setStateInformation (oldProject.getData(), (int) oldProject.getSize());
        check (loaded.voiceParam->getIndex() == 0 && loaded.voiceParam->getCurrentChoiceName() == "Soft Pad", "a saved project still loads its sound");

        // Settings: Preview sound goes off and comes back to the sound chosen last
        *processor.voiceParam = 3; // Organ
        runBlock (processor);
        processor.togglePreviewSound();
        check (processor.voiceParam->getIndex() == PadSynth::Off, "Preview sound off");
        settle (80);
        snapshot ("sound_off");
        runBlock (processor);
        processor.togglePreviewSound();
        check (processor.voiceParam->getIndex() == 3, "and on again: the sound that was chosen last (Organ)");
        (void) ui;
        *processor.voiceParam = 0;
        runBlock (processor);
    }

    {
        std::printf ("tooltips\n");
        auto& ui = *static_cast<ProgrezzorEditor*> (editor.get());
        auto& lf = static_cast<theme::LookAndFeel&> (ui.getLookAndFeel());
        const juce::String tip = "Preview sound. Choose Off when Progrezzor plays another instrument.";
        const juce::Point<int> mouse (400, 200);
        const auto bounds = lf.getTooltipBounds (tip, mouse, { 0, 0, 1000, 560 });
        check (bounds.getWidth() <= 250 && bounds.getHeight() > 60, "a tooltip is narrow and takes several lines");
        check (! bounds.contains (mouse) && (bounds.getX() - mouse.x >= 20 || mouse.x - bounds.getRight() >= 20), "and it lies well away from the mouse");
        const auto edge = lf.getTooltipBounds (tip, { 990, 550 }, { 0, 0, 1000, 560 });
        check (edge.getRight() <= 1000 && edge.getBottom() <= 560 && ! edge.contains (juce::Point<int> (990, 550)), "also near the edge of the window");
        if (! prefix.empty())
        {
            juce::Image image (juce::Image::ARGB, bounds.getWidth() + 40, bounds.getHeight() + 40, true);
            juce::Graphics g (image);
            g.fillAll (theme::background);
            g.setOrigin (20, 20);
            lf.drawTooltip (g, tip, bounds.getWidth(), bounds.getHeight());
            juce::File file (prefix + "_tooltip.png");
            file.deleteFile();
            if (auto stream = file.createOutputStream())
                juce::PNGImageFormat().writeImageToStream (image, *stream);
        }

        // Off in Settings: the tooltip window goes away; kept for every project
        processor.setShowTooltips (false);
        settle (200);
        {
            ProgrezzorProcessor another;
            check (! another.getShowTooltips(), "the choice about tooltips is kept for every project");
        }
        processor.setShowTooltips (true);
        settle (200);
    }

    {
        std::printf ("ChordPro import\n");
        auto& bar = static_cast<ProgrezzorEditor*> (editor.get())->getBar();
        const juce::File folder (CHORDPRO_TEST_DIR);
        const auto mk = [] (int root, engine::ChordType type) { engine::Block b; b.chord = { root, type, engine::kAutoInversion }; b.beats = 4.0; return b; };
        const int oldKey = processor.keyParam->getIndex(), oldScale = processor.scaleParam->getIndex();
        const bool oldLock = processor.isLockedToScale();
        *processor.keyParam = 0;
        *processor.scaleParam = (int) engine::ScaleId::Major;
        *processor.lockParam = true;
        bar.setEditing (true);
        processor.setProgression ({ mk (0, engine::ChordType::Major), mk (7, engine::ChordType::Major) });
        settle (60);
        const auto barBefore = processor.getProgression();
        const auto silentBlocks = [&] { int n = 0; for (int i = 0; i < (int) processor.getProgression().size(); ++i) n += processor.isBlockSilent (i) ? 1 : 0; return n; };

        // Play: nothing can be imported, and a file dragged on is not wanted
        bar.setEditing (false);
        check (! bar.importChordProFile (folder.getChildFile ("simple.cho")) && ! bar.isInterestedInFileDrag ({ folder.getChildFile ("simple.cho").getFullPathName() }), "in Play nothing is imported");
        bar.setEditing (true);
        check (bar.isInterestedInFileDrag ({ folder.getChildFile ("simple.cho").getFullPathName() }) && ! bar.isInterestedInFileDrag ({ "/tmp/song.mid" })
                   && ! bar.isInterestedInFileDrag ({ "a.cho", "b.cho" }), "a ChordPro file can be dragged on, other files not");

        // A plain file: the chords are in the field as Quick-text, and the bar is not changed until Enter
        check (bar.importChordProFile (folder.getChildFile ("simple.cho")), "the file is read");
        settle (60);
        check (bar.isInTextMode() && bar.getTextEditor().getText() == "C Am F G C Am F G", "the chords are in the Quick-text field");
        check (processor.getProgression() == barBefore, "the bar is not changed before Enter");
        snapshot ("chordpro_text");
        bar.cancelQuickText();
        check (! bar.isInTextMode() && processor.getProgression() == barBefore, "Esc leaves everything as it was");

        bar.importChordProFile (folder.getChildFile ("simple.cho"));
        bar.applyQuickText();
        settle (60);
        check (! bar.isInTextMode() && processor.getProgression().size() == 8 && std::abs (processor.getProgression()[0].beats - 4.0) < 1.0e-9, "Enter makes eight blocks of one bar");
        processor.undo();
        check (processor.getProgression() == barBefore, "Undo brings the old bar back");

        // Parts: a list, and the one that is picked
        check (bar.readChordProText (folder.getChildFile ("sections.cho").loadFileAsString()) == 5, "the file with parts has a list of five");
        bar.useChordProPart (1);
        check (bar.isInTextMode() && bar.getTextEditor().getText() == "F G C", "the chorus that was picked is in the field");
        bar.cancelQuickText();
        bar.useChordProPart (4);
        check (bar.getTextEditor().getText() == "Dm G", "and the bridge");
        bar.cancelQuickText();

        // The key: asked about, never changed by itself
        bar.importChordProFile (folder.getChildFile ("key.cho"));
        settle (60);
        check (bar.getSwitchKeyButton().isVisible() && bar.getNotice().contains ("G major") && bar.getNotice().contains ("Switch the key?"), "the song's key is asked about");
        check (processor.keyParam->getIndex() == 0 && processor.getKey().scale == engine::ScaleId::Major, "the key is not changed by itself");
        snapshot ("chordpro_key");
        bar.getSwitchKeyButton().triggerClick();
        settle (60);
        check (processor.keyParam->getIndex() == 7 && processor.getKey().scale == engine::ScaleId::Major && ! bar.getSwitchKeyButton().isVisible(), "Switch key changes it to G major");
        check (bar.getTextEditor().getText() == "G Em C D", "the chords in the text are the same");
        bar.applyQuickText();
        check (processor.getProgression().size() == 4 && processor.getProgression()[0].chord.root == 0 && processor.getProgression()[3].chord.root == 7 && silentBlocks() == 0, "and they are I, vi, IV and V of G: nothing outside the key");
        processor.undo();
        // Already in that key: no question
        bar.importChordProFile (folder.getChildFile ("key.cho"));
        check (! bar.getSwitchKeyButton().isVisible(), "a song in the key we are in asks nothing");
        bar.cancelQuickText();
        // A minor key
        bar.importChordProFile (folder.getChildFile ("keyminor.cho"));
        check (bar.getSwitchKeyButton().isVisible() && bar.getNotice().contains ("F# minor"), "a minor key says so");
        bar.getSwitchKeyButton().triggerClick();
        settle (60);
        check (processor.keyParam->getIndex() == 6 && processor.getKey().scale == engine::ScaleId::Minor, "and switches to F# minor");
        bar.cancelQuickText();
        *processor.keyParam = 0;
        *processor.scaleParam = (int) engine::ScaleId::Major;

        // The text can be changed, and the lock and the marks work as in any Quick-text
        bar.importChordProFile (folder.getChildFile ("key.cho")); // G Em C D in C major: D is outside the key
        bar.getSwitchKeyButton().setVisible (false);
        bar.getTextEditor().setText ("G Em C D", false);
        bar.applyQuickText();
        settle (60);
        check (silentBlocks() == 1 && bar.getAllowButton().isVisible(), "a chord outside the key is silent with the lock on, and Allow is there");
        processor.undo();
        settle (60);

        // Odd chords are marked in red, the rest is used
        bar.importChordProFile (folder.getChildFile ("unknown.cho"));
        bar.applyQuickText();
        settle (60);
        check (bar.isInTextMode() && processor.getProgression().size() == 5 && bar.getNotice().isNotEmpty(), "unknown chords are marked, and the others are used");
        snapshot ("chordpro_unknown");
        bar.cancelQuickText();
        check (processor.getProgression() == barBefore, "Esc takes it all back");

        // An empty file, and too many chords
        bar.importChordProFile (folder.getChildFile ("empty.cho"));
        settle (60);
        check (bar.isInTextMode() && bar.getTextEditor().getText().isEmpty() && bar.getNotice().contains ("No chords found"), "a file without chords says so, and nothing is changed");
        bar.cancelQuickText();
        bar.importChordProFile (folder.getChildFile ("toomany.cho"));
        settle (60);
        check (bar.getNotice().contains ("first 32"), "too many chords: it says that only the first 32 fit");
        bar.applyQuickText();
        settle (60);
        check (processor.getProgression().size() == 32, "and the first 32 are used");
        processor.undo();

        processor.setProgression (barBefore);
        *processor.keyParam = oldKey;
        *processor.scaleParam = oldScale;
        *processor.lockParam = oldLock;
        settle (60);
    }

    {
        std::printf ("key guess\n");
        auto& bar = static_cast<ProgrezzorEditor*> (editor.get())->getBar();
        const juce::File folder (CHORDPRO_TEST_DIR);
        const int oldKey = processor.keyParam->getIndex(), oldScale = processor.scaleParam->getIndex();
        const bool oldLock = processor.isLockedToScale();
        const auto setKey = [&] (int tonic, engine::ScaleId scale) { *processor.keyParam = tonic; *processor.scaleParam = (int) scale; };
        const auto typeIn = [&] (const char* text)
        {
            bar.setEditing (true);
            if (! bar.isInTextMode())
                bar.startQuickText();
            bar.getTextEditor().setText (text, false);
            bar.applyQuickText();
            settle (60);
        };
        *processor.lockParam = true;
        processor.setProgression ({});
        setKey (0, engine::ScaleId::Major);

        // G D C Em Am typed in C Major: it asks, and nothing is changed
        typeIn ("G D C Em Am");
        check (bar.isAskingKey() && bar.getGuessedKey().tonic == 7 && bar.getGuessedKey().scale == engine::ScaleId::Major, "G D C Em Am typed in C Major: it asks about G Major");
        check (bar.getGuessSwitchButton().getButtonText() == "Switch to G Major" && bar.getGuessKeepButton().getButtonText() == "Keep C Major"
                   && bar.getNotice() == "These chords fit G Major.", "the question and the buttons say G Major and C Major");
        check (processor.keyParam->getIndex() == 0 && processor.getKey().scale == engine::ScaleId::Major, "the key is not changed by itself");
        check (processor.getProgression().size() == 5 && processor.getProgression()[0].chord.root == 7, "the chords are as typed (G is 7 above C)");
        snapshot ("keyguess_ask");

        // Switch: the key changes, the chords stay the same chords
        bar.getGuessSwitchButton().triggerClick();
        settle (60);
        check (! bar.isAskingKey() && processor.keyParam->getIndex() == 7 && processor.getKey().scale == engine::ScaleId::Major, "Switch changes the key to G Major");
        check (processor.getProgression()[0].chord.root == 0 && processor.getProgression()[1].chord.root == 7 && processor.getProgression()[2].chord.root == 5
                   && processor.getProgression()[3].chord.root == 9 && processor.getProgression()[4].chord.root == 2,
               "and the chords are the same chords: G D C Em Am are I V IV vi ii of G");
        int silent = 0;
        for (int i = 0; i < (int) processor.getProgression().size(); ++i)
            silent += processor.isBlockSilent (i) ? 1 : 0;
        check (silent == 0, "nothing is outside the key now");

        // Keep: nothing changes
        setKey (0, engine::ScaleId::Major);
        typeIn ("Am F C G");
        check (bar.isAskingKey() && bar.getGuessedKey().tonic == 9 && bar.getGuessedKey().scale == engine::ScaleId::Minor, "Am F C G in C Major: A Minor is asked about");
        check (bar.getGuessSwitchButton().getButtonText() == "Switch to A Minor", "\"Switch to A Minor\"");
        bar.getGuessKeepButton().triggerClick();
        settle (60);
        check (! bar.isAskingKey() && processor.keyParam->getIndex() == 0 && processor.getKey().scale == engine::ScaleId::Major && processor.getProgression()[0].chord.root == 9,
               "Keep: the key and the chords are as they were");

        // The same key as now: nothing happens
        setKey (9, engine::ScaleId::Minor);
        typeIn ("Am F C G");
        check (! bar.isAskingKey(), "the same key as the one we are in: no question");
        setKey (9, engine::ScaleId::Dorian);
        typeIn ("Am F C G");
        check (! bar.isAskingKey(), "a minor scale like Dorian counts as minor: no question");

        // A borrowed chord that fits no key: no guess
        setKey (0, engine::ScaleId::Major);
        typeIn ("C Ab G Bb");
        check (! bar.isAskingKey(), "chords that fit no key: no question, and the key stays");

        // Esc or a change in the bar answers the question
        typeIn ("G D C Em Am");
        check (bar.isAskingKey(), "asked again");
        processor.undo();
        bar.refresh();
        check (! bar.isAskingKey(), "a change in the bar answers the question");

        // ChordPro: a file that says its key is not guessed. One that does not is.
        setKey (0, engine::ScaleId::Major);
        bar.setEditing (true);
        bar.importChordProFile (folder.getChildFile ("key.cho")); // {key: G}
        bar.applyQuickText();
        settle (60);
        check (! bar.isAskingKey() && processor.keyParam->getIndex() == 0, "a file with {key:}: no guessing (it asked about its key in the text)");
        setKey (9, engine::ScaleId::Minor);
        bar.importChordProFile (folder.getChildFile ("simple.cho")); // C Am F G ..., no key
        bar.applyQuickText();
        settle (60);
        check (bar.isAskingKey() && bar.getGuessedKey().tonic == 0 && bar.getGuessedKey().scale == engine::ScaleId::Major, "a file without a key: the chords are guessed (C Major) when they are put in");
        bar.getGuessKeepButton().triggerClick();
        settle (60);

        processor.setProgression ({});
        setKey (oldKey, (engine::ScaleId) oldScale);
        *processor.lockParam = oldLock;
        settle (60);
    }

    {
        std::printf ("text export\n");
        auto& bar = static_cast<ProgrezzorEditor*> (editor.get())->getBar();
        const int oldKey = processor.keyParam->getIndex(), oldScale = processor.scaleParam->getIndex();
        const bool oldLock = processor.isLockedToScale();
        *processor.keyParam = 0;
        *processor.scaleParam = (int) engine::ScaleId::Major;
        *processor.lockParam = true;
        bar.setEditing (true);
        const auto mk = [] (int root, engine::ChordType type, double beats, int inversion = engine::kAutoInversion)
        {
            engine::Block b;
            b.chord = { root, type, inversion };
            b.beats = beats;
            return b;
        };
        engine::Block pause;
        pause.rest = true;
        pause.beats = 2.0;
        const engine::Progression original { mk (0, engine::ChordType::Major, 4), mk (9, engine::ChordType::Minor, 2), mk (5, engine::ChordType::Major, 2), mk (7, engine::ChordType::Major, 4, 1),
                                             pause, mk (2, engine::ChordType::Min7, 6), mk (4, engine::ChordType::Minor, 2) };
        const auto sameBar = [] (const engine::Progression& a, const engine::Progression& b)
        {
            if (a.size() != b.size())
                return false;
            for (size_t i = 0; i < a.size(); ++i)
                if (a[i].rest != b[i].rest || std::abs (a[i].beats - b[i].beats) > 1.0e-9 || (! a[i].rest && a[i].chord != b[i].chord))
                    return false;
            return true;
        };
        processor.setProgression (original);
        settle (60);

        check (bar.progressionAsText() == "| C | Am F | G/B | R:0.5 | Dm7:1.5 | Em:0.5 |", "the text of the bar");

        // Copy as text, and back in with Quick-text: the same bar
        bar.startQuickText();
        bar.getTextEditor().setText (bar.progressionAsText(), false);
        processor.setProgression ({});
        bar.applyQuickText();
        settle (60);
        check (sameBar (processor.getProgression(), original), "the copied text gives the same bar when it is put back with Quick-text");
        if (bar.isAskingKey())
            bar.getGuessKeepButton().triggerClick();
        settle (60);

        // Save ChordPro, and import the file again
        const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("progrezzor-test-export.cho");
        check (bar.saveChordProTo (file) && file.existsAsFile(), "the ChordPro file is saved");
        const auto text = file.loadFileAsString();
        check (text.contains ("{title: Progrezzor}") && text.contains ("{key: C}") && text.contains ("{start_of_grid shape=\"4x4\"}") && text.contains ("{end_of_grid}") && text.contains ("N.C."),
               "it has the title, the key and a grid with N.C. for the pause");
        processor.setProgression ({});
        check (bar.importChordProFile (file), "the file is read again");
        bar.applyQuickText();
        settle (60);
        check (sameBar (processor.getProgression(), original), "and it gives the same bar: half bars, an inversion and a pause");
        file.deleteFile();
        if (bar.isAskingKey())
            bar.getGuessKeepButton().triggerClick();
        settle (60);

        processor.setProgression ({});
        check (! bar.saveChordProTo (file), "an empty bar is not saved");
        processor.setProgression ({});
        *processor.keyParam = oldKey;
        *processor.scaleParam = oldScale;
        *processor.lockParam = oldLock;
        settle (60);
    }

    {
        std::printf ("keyboard: chord roots\n");
        const int oldKey = processor.keyParam->getIndex(), oldScale = processor.scaleParam->getIndex();
        const auto setKey = [&] (int tonic, int scale) { *processor.keyParam = tonic; *processor.scaleParam = scale; };
        const auto lit = [&] (int note) // the pads lit while a key is held (0 if it plays nothing), then it lets go
        {
            juce::MidiBuffer down, up;
            down.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 90), 0);
            runBlock (processor, down);
            const unsigned pads = processor.litPads();
            up.addEvent (juce::MidiMessage::noteOff (1, note), 0);
            runBlock (processor, up);
            return pads;
        };
        const auto padOf = [] (unsigned mask) { for (int i = 0; i < 7; ++i) if (mask == (1u << i)) return i; return -1; };

        // Standard: the white keys, as before
        check (processor.keyboardModeParam->getIndex() == 0, "the white keys are the standard");
        setKey (3, (int) engine::ScaleId::Major); // Eb Major
        check (padOf (lit (60)) == 0 && padOf (lit (62)) == 1 && padOf (lit (71)) == 6 && lit (61) == 0 && lit (63) == 0, "White keys: C to B are pad 1 to 7, black keys play nothing");

        *processor.keyboardModeParam = ProgrezzorProcessor::kKeyboardChordRoots;

        // C Major: the pads are C Dm Em F G Am Bdim, so the white keys, and the black keys play nothing
        setKey (0, (int) engine::ScaleId::Major);
        bool cMajor = true;
        const int cNotes[7] = { 0, 2, 4, 5, 7, 9, 11 };
        for (int octave = 1; octave <= 8; ++octave)
            for (int pad = 0; pad < 7; ++pad)
                if (12 * octave + cNotes[pad] <= 127)
                    cMajor &= padOf (lit (12 * octave + cNotes[pad])) == pad;
        check (cMajor, "C Major: the white keys play pad 1 to 7 in every octave");
        bool blackSilent = true;
        for (int octave = 1; octave <= 8; ++octave)
            for (int pitch : { 1, 3, 6, 8, 10 })
                if (12 * octave + pitch <= 127)
                    blackSilent &= lit (12 * octave + pitch) == 0;
        check (blackSilent, "C Major: the black keys play nothing");

        // D Minor: Dm Edim F Gm Am Bb C. Bb is a black key, and it plays a pad.
        setKey (2, (int) engine::ScaleId::Minor);
        check (padOf (lit (62)) == 0 && padOf (lit (64)) == 1 && padOf (lit (65)) == 2 && padOf (lit (67)) == 3 && padOf (lit (69)) == 4 && padOf (lit (70)) == 5 && padOf (lit (72)) == 6,
               "D Minor: D E F G A Bb C play pad 1 to 7, with Bb on a black key");
        check (padOf (lit (58)) == 5 && padOf (lit (82)) == 5 && padOf (lit (46)) == 5, "Bb in every octave");
        check (lit (61) == 0 && lit (63) == 0 && lit (66) == 0 && lit (68) == 0 && lit (71) == 0, "the keys with no chord play nothing (C#, Eb, F#, Ab, B)");

        // A key with black keys on its pads: Eb Major is Eb Fm Gm Ab Bb Cm Ddim
        setKey (3, (int) engine::ScaleId::Major);
        check (padOf (lit (63)) == 0 && padOf (lit (65)) == 1 && padOf (lit (67)) == 2 && padOf (lit (68)) == 3 && padOf (lit (70)) == 4 && padOf (lit (72)) == 5 && padOf (lit (74)) == 6,
               "Eb Major: Eb F G Ab Bb C D play pad 1 to 7 (three black keys)");
        check (lit (64) == 0 && lit (69) == 0 && lit (71) == 0 && lit (61) == 0 && lit (66) == 0, "and E, A, B, C#, F# play nothing");

        // A pad variant has the same root, so the same key plays it
        processor.setPadVariant (4, engine::ChordType::Dom7, engine::kAutoInversion);
        check (padOf (lit (70)) == 4, "a pad with a variant (Bb7) is still played by its root");
        processor.clearPadVariant (4);

        // A key that is held while the key changes lets go all the same
        setKey (0, (int) engine::ScaleId::Major);
        juce::MidiBuffer hold;
        hold.addEvent (juce::MidiMessage::noteOn (1, 62, (juce::uint8) 90), 0);
        runBlock (processor, hold);
        check (processor.soundingNotes() > 0, "a key is held");
        setKey (3, (int) engine::ScaleId::Major); // D is not a chord root in Eb Major
        juce::MidiBuffer letGo;
        letGo.addEvent (juce::MidiMessage::noteOff (1, 62), 0);
        runBlock (processor, letGo);
        runBlock (processor);
        check (processor.soundingNotes() == 0, "it lets go although the key changed, so nothing hangs");

        // Saved with the project
        juce::MemoryBlock savedMode;
        processor.getStateInformation (savedMode);
        ProgrezzorProcessor loaded;
        loaded.setStateInformation (savedMode.getData(), (int) savedMode.getSize());
        check (loaded.keyboardModeParam->getIndex() == 1, "Chord roots is saved with the project");
        *processor.keyboardModeParam = ProgrezzorProcessor::kKeyboardWhiteKeys;
        juce::MemoryBlock savedWhite;
        processor.getStateInformation (savedWhite);
        loaded.setStateInformation (savedWhite.getData(), (int) savedWhite.getSize());
        check (loaded.keyboardModeParam->getIndex() == 0, "and so are the white keys");

        // Learn is as it was in both
        *processor.keyboardModeParam = ProgrezzorProcessor::kKeyboardChordRoots;
        processor.setLearning (true);
        {
            juce::MidiBuffer down, up;
            down.addEvent (juce::MidiMessage::noteOn (1, 61, (juce::uint8) 90), 0);
            runBlock (processor, down);
            check (processor.litPads() == 0 && processor.soundingNotes() == 1, "Learn: a key plays its own note, in Chord roots too");
            up.addEvent (juce::MidiMessage::noteOff (1, 61), 0);
            runBlock (processor, up);
        }
        processor.setLearning (false);

        *processor.keyboardModeParam = ProgrezzorProcessor::kKeyboardWhiteKeys;
        setKey (oldKey, oldScale);
        settle (60);
    }

    {
        std::printf ("bass note\n");
        check (! processor.bassParam->get(), "the bass note is off as standard");
        *processor.bassParam = true;
        juce::MemoryBlock savedBass;
        processor.getStateInformation (savedBass);
        ProgrezzorProcessor withBass;
        withBass.setStateInformation (savedBass.getData(), (int) savedBass.getSize());
        check (withBass.bassParam->get(), "the bass note is saved with the project");
        *processor.bassParam = false;
        juce::MemoryBlock savedNoBass;
        processor.getStateInformation (savedNoBass);
        withBass.setStateInformation (savedNoBass.getData(), (int) savedNoBass.getSize());
        check (! withBass.bassParam->get(), "and so is off");

        processor.pressPad (0);
        runBlock (processor);
        check (processor.soundingNotes() == 3, "with the bass note off a pad has three notes");
        processor.releasePad (0);
        runBlock (processor);
        *processor.bassParam = true;
        processor.pressPad (0);
        runBlock (processor);
        check (processor.soundingNotes() == 4, "with it on a pad has the bass note too");
        snapshot ("bass_on");
        processor.releasePad (0);
        runBlock (processor);
        check (processor.soundingNotes() == 0, "no notes sound when the pad is let go");
        *processor.bassParam = false;
    }

    editor.reset();
    std::printf (failures == 0 ? "ALL OK\n" : "%d FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
