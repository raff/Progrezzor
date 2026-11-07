#pragma once

#include "PluginProcessor.h"
#include "engine/NextPads.h"
#include "ProgrezzorLookAndFeel.h"

// One big pad: the name of the chord, and it lights up while it sounds. Pressing it with the mouse plays it; letting go
// (also outside the pad) stops it. Drag it down into the bar to add the chord there. The gear (on mouseover) opens the
// variants of the pad.
class PadButton : public juce::Component
{
public:
    std::function<void()> onPress;
    std::function<void()> onRelease;
    std::function<void()> onGear;
    std::function<void()> onDragStart;                    // the pad was pulled away: its sound stops
    std::function<void (juce::Point<int> screen)> onDragMove;
    std::function<void (juce::Point<int> screen)> onDragEnd;

    void setChordName (const juce::String& name);
    void setRoot (int pitchClass); // the note the chord is built on (0 = C ... 11 = B): it gives the pad its colour
    void setLit (bool nowLit);
    void setHasVariant (bool nowHasVariant);
    void setSuggested (bool nowSuggested); // fits well after the chord played last: a soft glow, not the full light of a pad that sounds
    bool isSuggested() const { return suggested; }
    void setDragEnabled (bool nowEnabled) { dragEnabled = nowEnabled; } // pulling a pad down into the bar is editing
    bool isHeldByMouse() const { return heldByMouse; }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    juce::Rectangle<int> gearArea() const { return { getWidth() - 34, 6, 28, 28 }; }

    juce::String chordName;
    bool lit = false;
    bool heldByMouse = false;
    bool dragging = false;
    bool hasVariant = false;
    bool suggested = false;
    int rootPitchClass = 0;
    bool dragEnabled = false;
};

// The Suggest button: a die and the word. One click fills the bar with a whole progression.
class SuggestButton : public juce::Button
{
public:
    SuggestButton() : juce::Button ("Suggest") {}
    void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;
};

// The Lock to scale button: a padlock that is lit while the lock is on
class LockToggle : public juce::Button
{
public:
    LockToggle() : juce::Button ("Lock to scale") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;
};

// The Play button of the bar: a triangle, lit while the progression plays. With Play off the bar can be edited
class PlayToggle : public juce::Button
{
public:
    PlayToggle() : juce::Button ("Play") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;
};

// A small button of the bar: a plus, an arrow for undo or redo, or a word
class BarButton : public juce::Button
{
public:
    enum class Icon { None, Plus, Minus, Undo, Redo, Pencil, Gear };
    BarButton (const juce::String& name, Icon i) : juce::Button (name), icon (i) {}
    void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;
    void setAccented (bool nowAccented) { accented = nowAccented; } // filled with the accent colour: the one thing to press

private:
    Icon icon;
    bool accented = false;
};

// The Drag tile: a small MIDI file. Drag it into the DAW (the file is written when the drag starts). It is the operating
// system's own drag, so the DAW takes the file as it takes one from a file manager.
class DragTile : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<juce::File()> makeFile; // writes the file; a file that does not exist if there is nothing to write

    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragStarted = false; }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void setReady (bool nowReady);

private:
    bool ready = false;
    bool dragStarted = false;
};

// A block of the progression: the name of its chord. Holding it plays the chord. Its width follows its length.
//   - drag the right edge: the length (steps of half a bar)
//   - drag the block: move it (Ctrl, Option on Mac: copy it)
//   - right-click: delete. Click: select (Delete removes it). A chord dragged from a pad onto it replaces it.
//   - the lock (on mouseover, always when locked) keeps the block when Suggest replaces the rest
class BlockButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void()> onPress;
    std::function<void()> onRelease;
    std::function<void()> onSelect;       // toggles the selection
    std::function<void()> onToggleLock;
    std::function<void()> onDelete;
    std::function<void (int steps)> onOctave; // the arrows (in Edit, on mouseover): +1 up, -1 down
    std::function<void()> onResizeStart;
    std::function<void (int dx)> onResizeDrag;
    std::function<void()> onResizeEnd;
    std::function<void (juce::Point<int> centre)> onMoveDrag;                 // centre of the block in the bar
    std::function<void (juce::Point<int> centre, bool copy)> onMoveEnd;

    void setChordName (const juce::String& name);
    void setPlaying (bool nowPlaying); // the loop is playing this block
    void setSelected (bool nowSelected);
    void setRoot (int pitchClass); // gives the block the colour of its chord
    void setLength (double beats, double barBeats); // shown small on the block: 1 bar, 1/2 bar, 2 bars
    static juce::String lengthText (double beats, double barBeats);
    void setDropTarget (bool nowTarget); // a chord from a pad is held over it and would replace it
    void setLocked (bool nowLocked);
    void setOutsideScale (bool nowOutside); // Lock to scale is on and the chord does not fit: marked, but it still plays
    bool isOutsideScale() const { return outsideScale; }
    void setOctave (int nowOctave); // shown as a small mark when it is not 0
    int getOctave() const { return octave; }
    void setSilent (bool nowSilent); // Quick-text disabled it (outside the scale, lock on): crossed out, and it makes no sound
    void setEditable (bool nowEditable); // in Play a block only plays its chord when it is pressed
    bool isHeldByMouse() const { return heldByMouse; }
    void setForceHover (bool nowForced) { forceHover = nowForced; repaint(); } // for the tests: draw as if the mouse is over it

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    enum class Mode { None, Press, Resize, Move };

    juce::Rectangle<int> lockArea() const { return { 2, 2, 26, 26 }; }
    // The two arrows lie over each other (up above, down below) just inside the right edge, which is for the length
    // The two small arrows for the octave of this block: one in the top right corner, one in the bottom right corner. They are
    // small, so that a click on the chord itself never hits them.
    int arrowSize() const { return std::min (16, getHeight() / 2 - 3); }
    juce::Rectangle<int> octaveUpArea() const { return { getWidth() - 30, 3, 18, arrowSize() }; }
    juce::Rectangle<int> octaveDownArea() const { return { getWidth() - 30, getHeight() - 9 - arrowSize(), 18, arrowSize() }; }
    bool inEdge (juce::Point<int> p) const { return p.x >= getWidth() - 10; }

    juce::String chordName;
    bool heldByMouse = false;
    bool playing = false;
    bool selected = false;
    bool dropTarget = false;
    bool forceHover = false;
    bool hovered() const { return forceHover || isMouseOver(); }
    int rootPitchClass = 0;
    juce::String lengthLabel;
    bool locked = false;
    bool outsideScale = false;
    bool silent = false;
    int octave = 0;
    bool editable = false;
    bool wasSelectedAtDown = false;
    Mode mode = Mode::None;
    int moveStartX = 0, moveStartY = 0;
};

// The bottom bar: the blocks of the progression side by side
class ProgressionBar : public juce::Component
{
public:
    explicit ProgressionBar (ProgrezzorProcessor&);
    ~ProgressionBar() override;

    // Rebuilds the blocks from the processor's progression
    void refresh();

    // Play or Edit. In Edit the blocks can be moved, resized, locked and deleted, and the loop is silent. Back in Play the
    // selection is dropped. (Also turns the Edit button.)
    void setEditing (bool nowEditing);
    bool isEditing() const { return editing; }

    // Greys out Undo, Redo and Capture when they have nothing to do
    void updateButtons();

    // Marks the block that the loop plays now (-1 for none)
    void setPlayingBlock (int index);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override { clearSelection(); }

    // The selected block (-1 for none). A click on a pad replaces its chord.
    int getSelected() const { return selected; }
    void clearSelection();
    bool replaceBlock (int index, const engine::Chord&); // false if there is no such block
    bool deleteSelected();

    // Dragging a chord in from a pad: where it would land
    bool containsScreenPoint (juce::Point<int> screen) const;
    int gapAtScreenPoint (juce::Point<int> screen) const;
    int blockAtScreenPoint (juce::Point<int> screen) const; // over the middle of a block: that block (it would be replaced), else -1
    void setDragActive (bool nowActive); // a chord is pulled from a pad: the bar shows that it is where it can land
    void showDropTarget (int block);                         // lights that block up, -1 for none
    void showDropGap (int gap); // -1 for none

    // Quick-text (inside Edit): the bar becomes one line of text. Enter uses it, Esc throws it away. Only the progression
    // changes, never the pads. A word that cannot be read is marked red; the rest is used anyway.
    bool isInTextMode() const { return textMode; }
    void startQuickText();
    void applyQuickText();  // Enter
    void cancelQuickText(); // Esc: the bar is as it was when the text was opened
    juce::TextEditor& getTextEditor() { return textEditor; }
    BarButton& getQuickTextButton() { return quickTextButton; }
    BarButton& getAllowButton() { return allowButton; }
    const juce::String& getNotice() const { return noticeText; }

    DragTile& getDragTile() { return dragTile; }

    // For the tests
    int numBlocks() const { return blocks.size(); }
    BlockButton* getBlock (int i) { return blocks[i]; }
    void shiftGeneralOctave (int steps);
    void showSettingsMenu();
    void askToReplace();  // Suggest was pressed and would overwrite chords: ask in the message line
    void cancelAsk();
    juce::Rectangle<int> getTrayBounds() const { return trayBounds; }
    BarButton& getSettingsButton() { return settingsButton; }
    const juce::String& getRegisterLabel() const { return registerLabel; }
    void chooseRegister (int index); // Settings: Low (0), Mid (1) or High (2)
    BarButton& getOctaveUpButton() { return octaveUpButton; }
    BarButton& getOctaveDownButton() { return octaveDownButton; }
    const juce::String& getOctaveLabel() const { return octaveLabel; }
    BarButton& getReplaceButton() { return replaceButton; }

private:
    int gapAt (juce::Point<int> p) const;
    void select (int index);
    void captureLastChords();
    void leaveTextMode();
    void updateTextFeedback();
    void updateNotice();
    void updateVisibility();

    class TextMarks;

    ProgrezzorProcessor& processor;
    PlayToggle playButton;
    bool lastPlayOn = false;                    // to see when Play changes, which switches Edit
    bool editing = false;
    BarButton replaceButton { "Replace", BarButton::Icon::None };
    bool askingReplace = false;
    int askedAtVersion = -1;
    BarButton undoButton { "Undo", BarButton::Icon::Undo };
    BarButton redoButton { "Redo", BarButton::Icon::Redo };
    BarButton captureButton { "Capture", BarButton::Icon::None };
    BarButton quickTextButton { "Text", BarButton::Icon::Pencil };
    BarButton allowButton { "Allow", BarButton::Icon::None };
    BarButton settingsButton { "Settings", BarButton::Icon::Gear };
    BarButton octaveDownButton { "Oct-", BarButton::Icon::Minus };
    BarButton octaveUpButton { "Oct+", BarButton::Icon::Plus };
    juce::Rectangle<int> octaveLabelBounds;
    juce::String octaveLabel, registerLabel;
    juce::TextEditor textEditor;
    std::unique_ptr<TextMarks> textMarks;
    bool textMode = false;
    engine::Progression progressionBeforeText;
    juce::Array<juce::Range<int>> problemRanges; // character positions in the text
    juce::String noticeText;                     // the short message under the blocks or the text
    bool noticeIsWarning = false;
    bool noticeHasAllow = false;
    int silentCount = 0;
    juce::Rectangle<int> noticeBounds;
    DragTile dragTile;
    juce::OwnedArray<BlockButton> blocks;
    static constexpr double kPixelsPerBar = 124.0;     // how wide one bar is on a block, when there is room
    static constexpr double kLeastPixelsPerBar = 92.0; // and the narrowest it gets when one block is pulled long
    juce::Rectangle<int> trayBounds, labelBounds;
    bool dragActive = false;
    std::vector<juce::Rectangle<int>> slots;    // where the blocks belong (the one that is dragged may be elsewhere)
    int playingBlock = -1;
    int selected = -1;
    int dropGap = -1;
    bool resizing = false;                      // a length is being dragged
    int resizeStartWidth = 0;
    double blockAreaWidth = 0.0;
    double scaleFor (const engine::Progression&) const; // pixels per beat for these blocks
    engine::Progression beforeGesture;          // the progression when a drag started, for one single Undo step
};

class ProgrezzorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ProgrezzorEditor (ProgrezzorProcessor&);
    ~ProgrezzorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    ProgressionBar& getBar() { return bar; }
    PadButton& getPad (int i) { return pads[(size_t) i]; }
    juce::Button& getSuggestButton() { return suggestButton; }
    void showVariantMenu (int pad);
    void saveMidi(); // the Save MIDI button
    BarButton& getSaveButton() { return saveButton; }
    LockToggle& getLockButton() { return lockButton; }

private:
    class DragGhost;

    void timerCallback() override;
    void updateNames();
    void updateSuggestedPads();
    void padPressed (int pad);

    ProgrezzorProcessor& processor;
    theme::LookAndFeel look;

    juce::Label title;
    juce::ComboBox keyBox, scaleBox, moodBox, voiceBox;
    std::unique_ptr<juce::ComboBoxParameterAttachment> keyAttachment, scaleAttachment, moodAttachment, voiceAttachment;

    LockToggle lockButton;
    std::unique_ptr<juce::ButtonParameterAttachment> lockAttachment;
    juce::TooltipWindow tooltipWindow { this, 600 };
    SuggestButton suggestButton;
    juce::Rectangle<int> padLabel;
    BarButton saveButton { "Save MIDI", BarButton::Icon::None };
    BarButton folderButton { "Open Folder", BarButton::Icon::None };
    std::array<PadButton, ProgrezzorProcessor::kNumPads> pads;
    ProgressionBar bar;
    std::unique_ptr<DragGhost> ghost;
    int shownKey = -1, shownScale = -1, shownProgression = -1, shownVariant = -1, shownLock = -1;
};
