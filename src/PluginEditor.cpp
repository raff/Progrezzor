#include "PluginEditor.h"

// Small icons, drawn as paths (no emoji fonts needed)
namespace
{
void drawLock (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour colour, bool closed)
{
    const float w = r.getWidth(), h = r.getHeight();
    const float cx = r.getX() + w * 0.5f;
    const float bodyTop = r.getY() + h * 0.48f;

    g.setColour (colour);
    g.fillRoundedRectangle (r.getX() + w * 0.2f, bodyTop, w * 0.6f, h * 0.42f, 2.0f);

    const float rx = w * 0.17f, cy = r.getY() + h * 0.32f;
    juce::Path shackle;
    shackle.addCentredArc (cx, cy, rx, h * 0.2f, 0.0f, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
    shackle.lineTo (cx + rx, closed ? bodyTop : cy);
    shackle.startNewSubPath (cx - rx, cy);
    shackle.lineTo (cx - rx, bodyTop);
    g.strokePath (shackle, juce::PathStrokeType (1.7f));
}

// "+1" or "-1": how many octaves a chord is moved. Nothing for 0.
juce::String octaveMark (int octave)
{
    if (octave == 0)
        return {};
    return (octave > 0 ? "+" : "-") + juce::String (std::abs (octave)) + " oct";
}

void drawArrow (juce::Graphics& g, juce::Rectangle<float> r, bool up, juce::Colour colour)
{
    juce::Path triangle;
    const auto c = r.getCentre();
    const float w = std::min (r.getWidth(), 11.0f), h = w * 0.6f;
    if (up)
        triangle.addTriangle (c.x - w * 0.5f, c.y + h * 0.5f, c.x + w * 0.5f, c.y + h * 0.5f, c.x, c.y - h * 0.5f);
    else
        triangle.addTriangle (c.x - w * 0.5f, c.y - h * 0.5f, c.x + w * 0.5f, c.y - h * 0.5f, c.x, c.y + h * 0.5f);
    g.setColour (colour);
    g.fillPath (triangle);
}

void drawGear (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour colour)
{
    const auto centre = r.getCentre();
    const float size = std::min (r.getWidth(), r.getHeight());
    g.setColour (colour);
    g.drawEllipse (juce::Rectangle<float> (size * 0.52f, size * 0.52f).withCentre (centre), 2.0f);
    for (int tooth = 0; tooth < 8; ++tooth)
    {
        const float angle = (float) tooth * juce::MathConstants<float>::twoPi / 8.0f;
        const juce::Point<float> direction (std::sin (angle), -std::cos (angle));
        g.drawLine (juce::Line<float> (centre + direction * (size * 0.3f), centre + direction * (size * 0.43f)), 2.6f);
    }
}
}

// ---- PadButton ---------------------------------------------------------------------------------------------------

void PadButton::setChordName (const juce::String& name)
{
    if (name != chordName)
    {
        chordName = name;
        repaint();
    }
}

void PadButton::setNumeral (const juce::String& newNumeral)
{
    if (newNumeral != numeral)
    {
        numeral = newNumeral;
        repaint();
    }
}

void PadButton::setLit (bool nowLit)
{
    if (nowLit != lit)
    {
        lit = nowLit;
        repaint();
    }
}

void PadButton::setHasVariant (bool nowHasVariant)
{
    if (nowHasVariant != hasVariant)
    {
        hasVariant = nowHasVariant;
        repaint();
    }
}

void PadButton::setSuggested (bool nowSuggested)
{
    if (nowSuggested != suggested)
    {
        suggested = nowSuggested;
        repaint();
    }
}

void PadButton::setRoot (int pitchClass)
{
    if (pitchClass != rootPitchClass)
    {
        rootPitchClass = pitchClass;
        repaint();
    }
}

void PadButton::paint (juce::Graphics& g)
{
    const bool on = lit || heldByMouse;
    const auto colour = theme::chordColour (rootPitchClass);
    const auto bounds = getLocalBounds().toFloat().reduced (5.0f, 4.0f);
    const float lip = juce::jlimit (5.0f, 9.0f, bounds.getHeight() * 0.035f);
    const float corner = 15.0f;

    const auto rest = colour.withMultipliedBrightness (0.80f);
    const auto base = on ? rest.brighter (0.10f) : rest; // a pad that plays is only a little brighter, with a faint glow
    if (on)
        theme::drawGlow (g, bounds.withTrimmedBottom (lip), colour, 7.0f, corner, 0.28f);

    const auto face = theme::drawKey (g, bounds, base, corner, lip, on, isMouseOver());

    if (suggested && ! on) // fits well after the last chord: a bright edge
    {
        g.setColour (theme::accent.withAlpha (0.9f));
        g.drawRoundedRectangle (face, corner, 3.0f);
    }

    // A small light in the corner, and grip dots at the bottom, like the keys of a drum machine
    const float led = 5.5f;
    const juce::Rectangle<float> light (face.getX() + 14.0f, face.getY() + 14.0f, led * 2.0f, led * 2.0f);
    g.setColour (on ? theme::shine.withAlpha (0.9f) : base.darker (0.5f));
    g.fillEllipse (light);
    g.setColour (theme::textOn (base).withAlpha (0.22f));
    for (int dot = -2; dot <= 2; ++dot)
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ face.getCentreX() + (float) dot * 12.0f, face.getBottom() - 15.0f }));

    const float height = juce::jlimit (16.0f, 64.0f, std::min ((float) getHeight() * 0.24f, (float) getWidth() * 0.28f));
    if (auto* l = dynamic_cast<theme::LookAndFeel*> (&getLookAndFeel()))
        g.setFont (l->titleFont (height));
    const auto textArea = face.reduced (6.0f, 10.0f).toNearestInt();
    const auto ink = theme::textOn (base);
    g.setColour (theme::shade.withAlpha (ink.getBrightness() > 0.5f ? 0.28f : 0.0f));
    g.drawFittedText (chordName, textArea.translated (0, 2), juce::Justification::centred, 1, 0.6f);
    g.setColour (ink);
    g.drawFittedText (chordName, textArea, juce::Justification::centred, 1, 0.6f);

    // Where the chord sits in the key (I, ii, III ...), small at the top
    if (numeral.isNotEmpty())
        if (auto* l = dynamic_cast<theme::LookAndFeel*> (&getLookAndFeel()))
        {
            g.setFont (l->titleFont (juce::jlimit (13.0f, 22.0f, (float) getWidth() * 0.16f)));
            g.setColour (ink.withAlpha (0.78f));
            g.drawText (numeral, face.withTrimmedTop (9.0f).withHeight (30.0f).toNearestInt(), juce::Justification::centredTop);
        }

    // The gear shows on mouseover (and stays a little visible when the pad plays another kind of chord)
    if (isMouseOver() || hasVariant)
        drawGear (g, gearArea().toFloat().reduced (3.0f), ink.withAlpha (isMouseOver() ? 0.9f : 0.5f));
}

void PadButton::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() || heldByMouse)
        return;

    if (gearArea().contains (e.getPosition()))
    {
        if (onGear)
            onGear();
        return;
    }

    heldByMouse = true;
    dragging = false;
    repaint();
    if (onPress)
        onPress();
}

void PadButton::mouseDrag (const juce::MouseEvent& e)
{
    // Pulled away from the pad: the sound stops and the chord follows the mouse until it is let go
    if (dragEnabled && heldByMouse && ! dragging && e.getDistanceFromDragStart() > 12)
    {
        dragging = true;
        heldByMouse = false;
        repaint();
        if (onRelease)
            onRelease();
        if (onDragStart)
            onDragStart();
    }

    if (dragging && onDragMove)
        onDragMove (e.getScreenPosition());
}

void PadButton::mouseUp (const juce::MouseEvent& e)
{
    if (dragging)
    {
        dragging = false;
        if (onDragEnd)
            onDragEnd (e.getScreenPosition());
        return;
    }

    if (! heldByMouse)
        return;

    heldByMouse = false;
    repaint();
    if (onRelease)
        onRelease();
}

// ---- SuggestButton -------------------------------------------------------------------------------------------------

void SuggestButton::paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto face = theme::drawKey (g, bounds, theme::accent, 12.0f, 5.0f, isButtonDown, isMouseOver);

    // From warm orange to hot pink, across the key
    g.setGradientFill (juce::ColourGradient (theme::accent.withAlpha (0.0f), face.getX(), 0.0f, theme::hotPink.withAlpha (0.85f), face.getRight(), 0.0f, false));
    g.fillRoundedRectangle (face, 12.0f);
    g.setColour (theme::shine.withAlpha (0.28f));
    g.fillRoundedRectangle (face.getX() + 8.0f, face.getY() + 2.0f, face.getWidth() - 16.0f, 2.2f, 1.1f);

    // A die: a white rounded square with five dots
    const float size = std::min (face.getHeight() - 10.0f, 26.0f);
    const juce::Rectangle<float> die (face.getX() + 14.0f, face.getCentreY() - size * 0.5f, size, size);
    g.setColour (theme::shine);
    g.fillRoundedRectangle (die, size * 0.24f);
    g.setColour (theme::onAccent);
    const float dot = size * 0.1f;
    for (const auto& p : { juce::Point<float> (0.27f, 0.27f), juce::Point<float> (0.73f, 0.27f), juce::Point<float> (0.5f, 0.5f),
                           juce::Point<float> (0.27f, 0.73f), juce::Point<float> (0.73f, 0.73f) })
        g.fillEllipse (juce::Rectangle<float> (dot * 2.0f, dot * 2.0f).withCentre ({ die.getX() + p.x * size, die.getY() + p.y * size }));

    if (auto* l = dynamic_cast<theme::LookAndFeel*> (&getLookAndFeel()))
        g.setFont (l->titleFont (18.0f));
    g.setColour (theme::onAccent);
    g.drawText ("Suggest", juce::Rectangle<float> (die.getRight() + 10.0f, face.getY(), face.getRight() - die.getRight() - 14.0f, face.getHeight()),
                juce::Justification::centredLeft);
}

// ---- LockToggle ----------------------------------------------------------------------------------------------------

void LockToggle::paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown)
{
    const bool on = getToggleState();
    // Grey like the other buttons, with the same light grey icon; the shackle is closed when the lock is on, open when it is off
    const auto face = theme::drawKey (g, getLocalBounds().toFloat(), theme::pad, 10.0f, 4.0f, isButtonDown, isMouseOver);
    drawLock (g, face.withSizeKeepingCentre (face.getHeight() * 0.58f, face.getHeight() * 0.58f),
              isMouseOver ? theme::text : theme::dimText.brighter (0.2f), on);
}

// ---- PlayToggle ----------------------------------------------------------------------------------------------------

void PlayToggle::paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown)
{
    const bool on = getToggleState();
    const auto bounds = getLocalBounds().toFloat();
    if (on)
        theme::drawGlow (g, bounds.reduced (5.0f, 6.0f), theme::go, 7.0f, 14.0f, 0.3f);
    const bool usable = isEnabled();
    const auto face = theme::drawKey (g, bounds, on ? theme::go : (usable ? theme::pad : theme::pad.darker (0.3f)), 14.0f, 6.0f,
                                      isButtonDown && usable, isMouseOver && usable);

    // The icon says what a click does: a triangle (start) while it is stopped, a square (stop) while it plays
    const float size = std::min (face.getWidth(), face.getHeight()) * 0.36f;
    g.setColour (on ? theme::onAccent : (usable ? theme::dimText : theme::dimText.withAlpha (0.4f)));
    if (on)
    {
        g.fillRoundedRectangle (juce::Rectangle<float> (size * 1.05f, size * 1.05f).withCentre (face.getCentre()), size * 0.16f);
    }
    else
    {
        juce::Path triangle;
        triangle.addTriangle (face.getCentreX() - size * 0.4f, face.getCentreY() - size * 0.62f,
                              face.getCentreX() - size * 0.4f, face.getCentreY() + size * 0.62f,
                              face.getCentreX() + size * 0.72f, face.getCentreY());
        g.fillPath (triangle.createPathWithRoundedCorners (3.0f));
    }
}

// ---- ArpToggle -----------------------------------------------------------------------------------------------------

void ArpToggle::paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown)
{
    const bool on = getToggleState();
    if (on)
        theme::drawGlow (g, getLocalBounds().toFloat().reduced (4.0f, 5.0f), theme::chordColour (7), 6.0f, 10.0f, 0.3f);
    const auto face = theme::drawKey (g, getLocalBounds().toFloat(), on ? theme::chordColour (7) : theme::pad, 10.0f, 4.0f, isButtonDown, isMouseOver);

    // Notes climbing up, one after the other: what the arp does
    const auto ink = on ? theme::onAccent : theme::dimText.brighter (0.2f);
    g.setColour (ink);
    const float x0 = face.getX() + face.getWidth() * 0.18f, x1 = face.getRight() - face.getWidth() * 0.18f;
    const float yTop = face.getY() + face.getHeight() * 0.20f, yLow = face.getY() + face.getHeight() * 0.46f;
    for (int i = 0; i < 4; ++i)
    {
        const float t = (float) i / 3.0f;
        g.fillEllipse (juce::Rectangle<float> (4.5f, 4.5f).withCentre ({ x0 + (x1 - x0) * t, yLow - (yLow - yTop) * t }));
    }
    if (auto* l = dynamic_cast<theme::LookAndFeel*> (&getLookAndFeel()))
        g.setFont (l->titleFont (13.0f));
    g.drawText ("Arp", face.withTrimmedTop (face.getHeight() * 0.5f).toNearestInt(), juce::Justification::centred);
}

// ---- LearnToggle ---------------------------------------------------------------------------------------------------

void LearnToggle::paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown)
{
    const bool on = getToggleState();
    if (on)
        theme::drawGlow (g, getLocalBounds().toFloat().reduced (4.0f, 5.0f), theme::chordColour (5), 6.0f, 10.0f, 0.3f);
    const auto face = theme::drawKey (g, getLocalBounds().toFloat(), on ? theme::chordColour (5) : theme::pad, 10.0f, 4.0f, isButtonDown, isMouseOver);

    // A graduation cap: a flat diamond with a tassel, big and bright so that it is seen at a glance
    const auto ink = on ? theme::onAccent : theme::dimText.brighter (0.2f); // the same grey as the icons on the other buttons
    const float k = 0.72f * std::min (face.getWidth() / 44.0f, face.getHeight() / 34.0f);
    const auto c = juce::Point<float> (face.getCentreX(), face.getY() + face.getHeight() * 0.30f);
    juce::Path cap;
    cap.startNewSubPath (c.x, c.y - 10.0f * k);
    cap.lineTo (c.x + 20.0f * k, c.y);
    cap.lineTo (c.x, c.y + 10.0f * k);
    cap.lineTo (c.x - 20.0f * k, c.y);
    cap.closeSubPath();
    g.setColour (ink);
    g.fillPath (cap);
    juce::Path crown; // the part that sits on the head, under the flat top
    crown.startNewSubPath (c.x - 10.0f * k, c.y + 5.0f * k);
    crown.lineTo (c.x - 10.0f * k, c.y + 11.0f * k);
    crown.quadraticTo (c.x, c.y + 16.0f * k, c.x + 10.0f * k, c.y + 11.0f * k);
    crown.lineTo (c.x + 10.0f * k, c.y + 5.0f * k);
    g.strokePath (crown, juce::PathStrokeType (2.0f * k));
    g.drawLine (c.x + 16.0f * k, c.y + 1.0f * k, c.x + 16.0f * k, c.y + 12.0f * k, 2.0f * k);
    if (auto* l = dynamic_cast<theme::LookAndFeel*> (&getLookAndFeel()))
        g.setFont (l->titleFont (13.0f));
    g.drawText ("Learn", face.withTrimmedTop (face.getHeight() * 0.62f).toNearestInt(), juce::Justification::centred);
}

// ---- BarButton -----------------------------------------------------------------------------------------------------

void BarButton::paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown)
{
    const bool enabled = isEnabled();
    const bool lit = accented || getToggleState();
    const auto base = lit ? theme::accent : (isMouseOver && enabled ? theme::padHover : theme::pad);
    const auto face = theme::drawKey (g, getLocalBounds().toFloat(), enabled ? base : theme::pad.darker (0.3f), 9.0f, 4.0f, isButtonDown && enabled, isMouseOver && enabled);
    const auto bounds = face;

    const auto colour = lit ? theme::onAccent
                                 : (enabled ? (isMouseOver ? theme::text : theme::dimText.brighter (0.2f)) : theme::dimText.withAlpha (0.35f));
    g.setColour (colour);
    const auto centre = bounds.getCentre();
    const float size = std::min (bounds.getWidth(), bounds.getHeight()) * 0.4f;

    switch (icon)
    {
        case Icon::Plus:
            g.drawLine (centre.x - size, centre.y, centre.x + size, centre.y, 2.4f);
            g.drawLine (centre.x, centre.y - size, centre.x, centre.y + size, 2.4f);
            break;

        case Icon::Dots:
            for (int i = -1; i <= 1; ++i)
                g.fillEllipse (juce::Rectangle<float> (3.6f, 3.6f).withCentre ({ centre.x, centre.y + (float) i * 6.0f }));
            break;

        case Icon::Gear:
            drawGear (g, bounds.reduced (bounds.getWidth() * 0.12f), colour);
            break;

        case Icon::Minus:
            g.drawLine (centre.x - size, centre.y, centre.x + size, centre.y, 2.4f);
            break;

        case Icon::Undo:
        case Icon::Redo:
        {
            // An arrow that curves round: Undo points left at the top, Redo mirrors it
            const float flip = icon == Icon::Undo ? 1.0f : -1.0f;
            const float radius = size * 0.85f;
            juce::Path arc;
            arc.addCentredArc (centre.x, centre.y + size * 0.1f, radius, radius, 0.0f,
                               flip > 0.0f ? 0.0f : -juce::MathConstants<float>::pi * 0.95f,
                               flip > 0.0f ? juce::MathConstants<float>::pi * 0.95f : 0.0f, true);
            g.strokePath (arc, juce::PathStrokeType (2.2f));

            const float tipX = centre.x, tipY = centre.y + size * 0.1f - radius;
            juce::Path head;
            head.addTriangle (tipX - flip * size * 0.5f, tipY, tipX + flip * size * 0.15f, tipY - size * 0.4f,
                              tipX + flip * size * 0.15f, tipY + size * 0.4f);
            g.fillPath (head);
            break;
        }

        case Icon::Pencil:
        {
            // A pencil, tip at the bottom left: drawn upright (tip down) and turned a quarter of the way round
            const float u = size * 0.09f;
            juce::Path pencil;
            pencil.addRoundedRectangle (-3.2f * u, -11.0f * u, 6.4f * u, 13.0f * u, 1.2f * u);
            pencil.addTriangle (-3.2f * u, 2.0f * u, 3.2f * u, 2.0f * u, 0.0f, 10.0f * u);
            pencil.applyTransform (juce::AffineTransform::rotation (juce::MathConstants<float>::pi * 0.25f).translated (centre.x + size * 0.1f, centre.y - size * 0.1f));
            g.strokePath (pencil, juce::PathStrokeType (1.8f, juce::PathStrokeType::mitered));
            break;
        }

        case Icon::None:
            if (auto* l = dynamic_cast<theme::LookAndFeel*> (&getLookAndFeel()))
                g.setFont (l->titleFont (15.0f));
            {
                auto labelArea = bounds.toNearestInt();
                if (recordDot)
                {
                    const auto label = getButtonText().isEmpty() ? getName() : getButtonText();
                    const float textWidth = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), label);
                    const float dot = 9.0f, gap = 6.0f;
                    const float x = bounds.getCentreX() - (textWidth + gap + dot) * 0.5f;
                    g.setColour (getToggleState() ? juce::Colour (0xffe0202a) : juce::Colour (0xffe0202a).withAlpha (enabled ? 0.75f : 0.3f));
                    g.fillEllipse (juce::Rectangle<float> (dot, dot).withCentre ({ x + dot * 0.5f, bounds.getCentreY() }));
                    labelArea = labelArea.withTrimmedLeft ((int) (dot + gap));
                }
                g.setColour (theme::shade.withAlpha (accented ? 0.0f : 0.35f));
                g.drawText (getButtonText().isEmpty() ? getName() : getButtonText(), labelArea.translated (0, 1), juce::Justification::centred);
                g.setColour (colour);
                g.drawText (getButtonText().isEmpty() ? getName() : getButtonText(), labelArea, juce::Justification::centred);
            }
            break;
    }
}

// ---- DragTile ------------------------------------------------------------------------------------------------------

void DragTile::setReady (bool nowReady)
{
    if (nowReady != ready)
    {
        ready = nowReady;
        setMouseCursor (ready ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void DragTile::paint (juce::Graphics& g)
{
    const bool hot = ready && isMouseOver();
    const auto bounds = getLocalBounds().toFloat();
    const auto face = theme::drawKey (g, bounds, hot ? theme::padHover : theme::pad, 11.0f, 5.0f, false, hot);

    // A page with a folded corner and the word MIDI, and "Drag" below it. Ready: the page is coloured in.
    const float pageHeight = face.getHeight() * 0.5f, pageWidth = pageHeight * 0.78f;
    const juce::Rectangle<float> page (face.getCentreX() - pageWidth * 0.5f, face.getY() + face.getHeight() * 0.1f, pageWidth, pageHeight);
    const float fold = pageWidth * 0.3f;
    juce::Path outline;
    outline.startNewSubPath (page.getX(), page.getY());
    outline.lineTo (page.getRight() - fold, page.getY());
    outline.lineTo (page.getRight(), page.getY() + fold);
    outline.lineTo (page.getRight(), page.getBottom());
    outline.lineTo (page.getX(), page.getBottom());
    outline.closeSubPath();
    const auto rounded = outline.createPathWithRoundedCorners (3.0f);
    if (ready)
    {
        g.setGradientFill (juce::ColourGradient (theme::chordColour (7), page.getX(), page.getY(), theme::chordColour (9), page.getRight(), page.getBottom(), false));
        g.fillPath (rounded);
    }
    const auto colour = ready ? theme::shine : theme::dimText.withAlpha (0.45f);
    g.setColour (colour);
    g.strokePath (rounded, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved));

    if (auto* l = dynamic_cast<theme::LookAndFeel*> (&getLookAndFeel()))
        g.setFont (l->titleFont (juce::jlimit (9.0f, 13.0f, pageWidth * 0.34f)));
    g.drawText ("MIDI", page.withTrimmedTop (fold * 0.8f).toNearestInt(), juce::Justification::centred);

    if (auto* l = dynamic_cast<theme::LookAndFeel*> (&getLookAndFeel()))
        g.setFont (l->titleFont (14.0f));
    g.setColour (ready ? theme::text : theme::dimText.withAlpha (0.5f));
    g.drawText ("Drag", face.toNearestInt().withTop (juce::roundToInt (page.getBottom())).reduced (2), juce::Justification::centred);
}

// The drag is the operating system's: the DAW takes the file as it takes one from a file manager
void DragTile::mouseDrag (const juce::MouseEvent& e)
{
    if (dragStarted || ! ready || e.getDistanceFromDragStart() <= 4 || ! makeFile)
        return;

    dragStarted = true;
    const auto file = makeFile();
    if (file.existsAsFile())
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
}

// ---- BlockButton ---------------------------------------------------------------------------------------------------

void BlockButton::setChordName (const juce::String& name)
{
    if (name != chordName)
    {
        chordName = name;
        repaint();
    }
}

void BlockButton::setPlaying (bool nowPlaying)
{
    if (nowPlaying != playing)
    {
        playing = nowPlaying;
        repaint();
    }
}

void BlockButton::setSelected (bool nowSelected)
{
    if (nowSelected != selected)
    {
        selected = nowSelected;
        repaint();
    }
}

// "1 bar", "1/2 bar", "1 1/2 bars", "2 bars"
juce::String BlockButton::lengthText (double beats, double barBeats)
{
    const double bars = beats / std::max (1.0, barBeats);
    const double halves = std::round (bars * 2.0);
    if (std::abs (bars * 2.0 - halves) < 0.02)
    {
        const int whole = (int) halves / 2;
        const bool half = ((int) halves % 2) != 0;
        juce::String text = whole > 0 ? juce::String (whole) : juce::String();
        if (half)
            text += (whole > 0 ? " 1/2" : "1/2");
        return text + (bars > 1.01 ? " bars" : " bar");
    }
    return juce::String (bars, 2) + " bars";
}

void BlockButton::setLength (double beats, double barBeats)
{
    const auto text = lengthText (beats, barBeats);
    if (text != lengthLabel)
    {
        lengthLabel = text;
        repaint();
    }
}

void BlockButton::setNumeral (const juce::String& nowNumeral)
{
    if (nowNumeral != numeral)
    {
        numeral = nowNumeral;
        repaint();
    }
}

void BlockButton::setLearn (bool nowDone, bool nowTarget)
{
    if (nowDone != learnDone || nowTarget != learnTarget)
    {
        learnDone = nowDone;
        learnTarget = nowTarget;
        repaint();
    }
}

void BlockButton::flashWrong()
{
    wrongUntil = juce::Time::getMillisecondCounter() + 500;
    repaint();
    juce::Component::SafePointer<BlockButton> safe (this);
    juce::Timer::callAfterDelay (530, [safe] { if (safe != nullptr) safe->repaint(); });
}

void BlockButton::setDropTarget (bool nowTarget)
{
    if (nowTarget != dropTarget)
    {
        dropTarget = nowTarget;
        repaint();
    }
}

void BlockButton::setLocked (bool nowLocked)
{
    if (nowLocked != locked)
    {
        locked = nowLocked;
        repaint();
    }
}

void BlockButton::setOutsideScale (bool nowOutside)
{
    if (nowOutside != outsideScale)
    {
        outsideScale = nowOutside;
        setTooltip (silent ? "Not in the scale, so it is silent. Press Allow to play it." : (outsideScale ? "This chord is not in the scale" : juce::String()));
        repaint();
    }
}

void BlockButton::setOctave (int nowOctave)
{
    if (nowOctave != octave)
    {
        octave = nowOctave;
        repaint();
    }
}

void BlockButton::setSilent (bool nowSilent)
{
    if (nowSilent != silent)
    {
        silent = nowSilent;
        setTooltip (silent ? "Not in the scale, so it is silent. Press Allow to play it." : (outsideScale ? "This chord is not in the scale" : juce::String()));
        repaint();
    }
}

void BlockButton::setEditable (bool nowEditable)
{
    if (nowEditable != editable)
    {
        editable = nowEditable;
        setMouseCursor (juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void BlockButton::setRoot (int pitchClass)
{
    if (pitchClass != rootPitchClass)
    {
        rootPitchClass = pitchClass;
        repaint();
    }
}

void BlockButton::paint (juce::Graphics& g)
{
    // A key in the colour of its chord (darker than the pads, so the pads stay the loud ones)
    const auto colour = theme::chordColour (rootPitchClass);
    const bool lit = heldByMouse || dropTarget;
    const auto outer = getLocalBounds().toFloat().reduced (1.0f, 0.0f);
    auto base = colour.withMultipliedBrightness (0.52f).withMultipliedSaturation (0.9f);
    if (playing || selected)
        base = colour.withMultipliedBrightness (0.72f);
    if (lit)
        base = theme::accent;

    const float lip = juce::jlimit (3.0f, 6.0f, outer.getHeight() * 0.09f);
    const auto bounds = theme::drawKey (g, outer, base, 9.0f, lip, heldByMouse, hovered());
    const auto body = outer.reduced (0.5f).withTrimmedBottom (3.0f); // the whole key, lip included

    // The block that plays: a crisp white outline right round the key, and a green light. A drop target and a selected
    // block have their own outlines.
    if (playing && ! lit)
    {
        g.setColour (theme::shine);
        g.drawRoundedRectangle (body.expanded (0.5f), 9.5f, 3.0f);
        const auto light = juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ bounds.getRight() - 12.0f, bounds.getY() + 12.0f });
        g.setColour (theme::go);
        g.fillEllipse (light);
        g.setColour (theme::shade.withAlpha (0.45f));
        g.drawEllipse (light, 1.0f);
    }
    if (juce::Time::getMillisecondCounter() < wrongUntil) // Learn: that was not it
    {
        g.setColour (theme::warn);
        g.drawRoundedRectangle (body.expanded (0.5f), 9.5f, 5.0f);
    }
    else if (learnTarget) // Learn: this is the chord to play now
    {
        const float pulse = 0.5f + 0.5f * std::sin ((float) juce::Time::getMillisecondCounter() * 0.008f);
        g.setColour (theme::accent.withAlpha (0.55f + 0.45f * pulse));
        g.drawRoundedRectangle (body.expanded (0.5f), 9.5f, 3.0f + 1.5f * pulse);
        juce::Path arrow; // a little arrow in the corner: play me
        const auto c = juce::Point<float> (bounds.getRight() - 14.0f, bounds.getY() + 14.0f);
        arrow.addTriangle (c.x - 5.0f, c.y - 6.0f, c.x - 5.0f, c.y + 6.0f, c.x + 6.0f, c.y);
        g.setColour (theme::accent);
        g.fillPath (arrow);
    }
    else if (learnDone) // Learn: played, a green mark
    {
        const auto c = juce::Point<float> (bounds.getRight() - 14.0f, bounds.getY() + 14.0f);
        g.setColour (theme::go);
        g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre (c));
        juce::Path tick;
        tick.startNewSubPath (c.x - 4.5f, c.y);
        tick.lineTo (c.x - 1.0f, c.y + 3.5f);
        tick.lineTo (c.x + 5.0f, c.y - 3.5f);
        g.setColour (theme::onAccent);
        g.strokePath (tick, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    if (dropTarget)
    {
        g.setColour (theme::accent.brighter (0.5f));
        g.drawRoundedRectangle (body.expanded (0.5f), 9.5f, 3.4f);
    }
    else if (selected && ! playing)
    {
        g.setColour (theme::accent);
        g.drawRoundedRectangle (body.expanded (0.5f), 9.5f, 2.6f);
    }

    if (outsideScale) // not in the scale: a dashed red edge and dimmer text
    {
        juce::Path edge;
        edge.addRoundedRectangle (bounds.reduced (3.0f), 6.0f);
        juce::Path dashed;
        const float dashes[] = { 5.0f, 4.0f };
        juce::PathStrokeType (1.6f).createDashedStroke (dashed, edge, dashes, 2);
        g.setColour (theme::warn);
        g.fillPath (dashed);
    }

    const auto ink = theme::textOn (base);
    g.setColour (lit ? theme::onAccent : (silent ? ink.withAlpha (0.4f) : (outsideScale ? ink.withAlpha (0.65f) : ink)));
    if (auto* l = dynamic_cast<theme::LookAndFeel*> (&getLookAndFeel()))
    {
        // The name and the length sit together in the middle of the face, not down at the bottom. They follow the height of
        // the block: big when there is room, smaller when many rows share the bar.
        const float nameSize = juce::jlimit (12.0f, 34.0f, bounds.getHeight() * 0.40f);
        const float lengthSize = juce::jlimit (10.0f, 15.0f, bounds.getHeight() * 0.22f);
        const float nameHeight = nameSize * 1.12f, lengthHeight = lengthSize * 1.2f;
        const float top = bounds.getCentreY() - (nameHeight + lengthHeight) * 0.5f;
        g.setFont (l->titleFont (nameSize));
        g.drawFittedText (chordName, juce::Rectangle<float> (bounds.getX() + 4.0f, top, bounds.getWidth() - 8.0f, nameHeight).toNearestInt(),
                          juce::Justification::centred, 1, 0.6f);

        // Under the name, in one line: where the chord sits in the key (I, ii, III ...), how long it lasts, and how far the chord
        // is moved (when it is). If the block is too narrow for everything, the numeral goes first, then the long form of the move.
        const auto lengthFont = l->labelFont (lengthSize);
        const auto boldFont = l->titleFont (lengthSize);
        const auto row = juce::Rectangle<float> (bounds.getX(), top + nameHeight, bounds.getWidth(), lengthHeight).toNearestInt();
        const juce::Colour lengthInk = lit ? theme::onAccent.withAlpha (0.85f) : ink.withAlpha (0.72f);
        const juce::Colour numeralInk = lit ? theme::onAccent : ink.withAlpha (0.95f);
        const juce::Colour markInk = lit ? theme::onAccent : theme::accent;

        struct Part { juce::String text; juce::Font font; juce::Colour colour; float width; };
        const auto make = [&] (bool withNumeral, bool shortMark)
        {
            std::vector<Part> parts;
            if (withNumeral && numeral.isNotEmpty())
                parts.push_back ({ numeral, boldFont, numeralInk, 0.0f });
            parts.push_back ({ lengthLabel, lengthFont, lengthInk, 0.0f });
            if (octave != 0)
            {
                auto mark = octaveMark (octave);
                if (shortMark)
                    mark = mark.upToFirstOccurrenceOf (" ", false, false);
                parts.push_back ({ mark, boldFont, markInk, 0.0f });
            }
            for (auto& part : parts)
                part.width = juce::GlyphArrangement::getStringWidth (part.font, part.text);
            return parts;
        };
        const auto totalWidth = [] (const std::vector<Part>& parts)
        {
            float total = 0.0f;
            for (const auto& part : parts)
                total += part.width;
            return total + 10.0f * (float) (parts.size() - 1);
        };

        auto parts = make (true, false);
        if (totalWidth (parts) > bounds.getWidth() - 8.0f)
            parts = make (true, true);
        if (totalWidth (parts) > bounds.getWidth() - 8.0f)
            parts = make (false, true);

        float x = (float) row.getCentreX() - totalWidth (parts) * 0.5f;
        for (const auto& part : parts)
        {
            g.setFont (part.font);
            g.setColour (part.colour);
            g.drawText (part.text, juce::Rectangle<float> (x, (float) row.getY(), part.width + 4.0f, (float) row.getHeight()).toNearestInt(), juce::Justification::centredLeft);
            x += part.width + 10.0f;
        }
    }

    if (silent) // crossed out: it keeps its place and its time, but makes no sound
    {
        g.setColour (theme::warn.withAlpha (0.8f));
        g.drawLine (10.0f, (float) getHeight() - 10.0f, (float) getWidth() - 10.0f, 10.0f, 1.6f);
    }

    // In Edit the lock shows on mouseover, and always on a block that is locked (in Play only on a locked one)
    if (locked || (editable && hovered()))
        drawLock (g, lockArea().toFloat().reduced (5.0f), locked ? (heldByMouse ? theme::onAccent : juce::Colours::white) : juce::Colours::white.withAlpha (0.75f), locked);

    // The arrows for the octave of this block, in Edit on mouseover
    if (editable && hovered())
    {
        const auto arrowColour = heldByMouse ? theme::onAccent : theme::text.withAlpha (0.85f);
        drawArrow (g, octaveUpArea().toFloat(), true, octave >= engine::kMaxOctaveShift ? arrowColour.withAlpha (0.3f) : arrowColour);
        drawArrow (g, octaveDownArea().toFloat(), false, octave <= -engine::kMaxOctaveShift ? arrowColour.withAlpha (0.3f) : arrowColour);
    }

    // The grip on the right edge, where the length is dragged
    if (editable && hovered())
    {
        const float x = (float) getWidth() - 6.0f, cy = (float) getHeight() * 0.5f;
        g.setColour (heldByMouse ? theme::onAccent : theme::dimText);
        g.drawLine (x, cy - 7.0f, x, cy + 7.0f, 2.0f);
    }
}

void BlockButton::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (editable && inEdge (e.getPosition()) ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
}

void BlockButton::mouseDown (const juce::MouseEvent& e)
{
    if (mode != Mode::None)
        return;

    if (! editable) // Play: the block only plays its chord while it is held
    {
        if (e.mods.isPopupMenu() || silent) // a silent block has no sound to hold
            return;

        mode = Mode::Press;
        heldByMouse = true;
        wasSelectedAtDown = false;
        repaint();
        if (onPress)
            onPress();
        return;
    }

    if (e.mods.isPopupMenu())
    {
        if (onDelete)
            onDelete();
        return;
    }

    if (lockArea().contains (e.getPosition()))
    {
        if (onToggleLock)
            onToggleLock();
        return;
    }

    if (octaveUpArea().contains (e.getPosition()) || octaveDownArea().contains (e.getPosition()))
    {
        if (onOctave)
            onOctave (octaveUpArea().contains (e.getPosition()) ? 1 : -1);
        return;
    }

    if (inEdge (e.getPosition()))
    {
        mode = Mode::Resize;
        if (onResizeStart)
            onResizeStart();
        return;
    }

    mode = Mode::Press;
    heldByMouse = true;
    wasSelectedAtDown = selected;
    moveStartX = getX();
    moveStartY = getY();
    repaint();
    if (! selected && onSelect)
        onSelect();
    if (onPress)
        onPress();
}

void BlockButton::mouseDrag (const juce::MouseEvent& e)
{
    if (mode == Mode::Resize)
    {
        if (onResizeDrag)
            onResizeDrag (e.getDistanceFromDragStartX());
        return;
    }

    // Pulled sideways: the sound stops and the block follows the mouse
    if (editable && mode == Mode::Press && std::abs (e.getDistanceFromDragStartX()) > 8)
    {
        mode = Mode::Move;
        heldByMouse = false;
        wasSelectedAtDown = true; // a drag is not a click: the selection stays as it is
        repaint();
        toFront (false);
        if (onRelease)
            onRelease();
    }

    if (mode == Mode::Move)
    {
        setTopLeftPosition (moveStartX + e.getDistanceFromDragStartX(), moveStartY + e.getDistanceFromDragStartY());
        if (onMoveDrag)
            onMoveDrag (getBounds().getCentre());
    }
}

void BlockButton::mouseUp (const juce::MouseEvent& e)
{
    const auto was = mode;
    mode = Mode::None;

    if (was == Mode::Resize)
    {
        if (onResizeEnd)
            onResizeEnd();
    }
    else if (was == Mode::Move)
    {
#if JUCE_MAC
        const bool copy = e.mods.isAltDown();
#else
        const bool copy = e.mods.isCtrlDown();
#endif
        if (onMoveEnd)
            onMoveEnd (getBounds().getCentre(), copy);
    }
    else if (was == Mode::Press)
    {
        heldByMouse = false;
        repaint();
        if (onRelease)
            onRelease();
        if (wasSelectedAtDown && onSelect) // a second click on the selected block lets go of it
            onSelect();
    }
}

// ---- ProgressionBar ------------------------------------------------------------------------------------------------

// Lies over the Quick-text field and marks the words that cannot be read: a red tint and a line under them. It does not
// take the mouse, so the field below works as usual.
class ProgressionBar::TextMarks : public juce::Component
{
public:
    explicit TextMarks (juce::TextEditor& e) : editor (e) { setInterceptsMouseClicks (false, false); }

    juce::Array<juce::Range<int>> ranges;

    void paint (juce::Graphics& g) override
    {
        for (const auto& range : ranges)
        {
            for (const auto& r : editor.getTextBounds (range))
            {
                const auto box = r.toFloat().expanded (1.0f, 0.0f);
                g.setColour (theme::warn.withAlpha (0.28f));
                g.fillRoundedRectangle (box, 3.0f);
                g.setColour (theme::warn);
                g.drawLine (box.getX(), box.getBottom() - 1.0f, box.getRight(), box.getBottom() - 1.0f, 2.0f);
            }
        }
    }

private:
    juce::TextEditor& editor;
};

// ---- ArpPanel ------------------------------------------------------------------------------------------------------

// The panel behind the Arp button: pick how the notes go (up, down, up and down, at random), how fast, and in what rhythm,
// shown as a ring of dots where the lit dots are the notes that play. Everything is a picture or a button, no numbers needed.
class ArpPanel : public juce::Component, private juce::Timer
{
public:
    explicit ArpPanel (ProgrezzorProcessor& p) : processor (p)
    {
        setSize (372, 352);
        startTimerHz (15);
    }

    void paint (juce::Graphics& g) override
    {
        auto* l = dynamic_cast<theme::LookAndFeel*> (&getLookAndFeel());
        const auto arp = processor.getArpSettings();

        // Title and the switch
        if (l != nullptr)
            g.setFont (l->titleFont (21.0f));
        g.setColour (theme::text);
        g.drawText ("Arp", 16, 12, 80, 28, juce::Justification::centredLeft);
        drawPill (g, onRect(), arp.on ? "On" : "Off", arp.on, true);

        const auto label = [&] (const juce::String& text, int y)
        {
            if (l != nullptr)
                g.setFont (l->labelFont (12.0f));
            g.setColour (theme::dimText);
            g.drawText (text, 16, y, 200, 16, juce::Justification::centredLeft);
        };

        // The pattern: four little pictures of how the notes move
        label ("PATTERN", 48);
        static const char* styleNames[engine::kNumArpStyles] = { "Up", "Down", "Up/Down", "Random" };
        for (int i = 0; i < engine::kNumArpStyles; ++i)
        {
            const auto r = styleRect (i).toFloat();
            const bool selected = arp.style == i;
            drawTile (g, r, selected);
            drawStyleIcon (g, r.withTrimmedBottom (18.0f).reduced (10.0f, 8.0f), i, selected);
            drawTileLabel (g, r, styleNames[i], selected);
        }

        // The speed: more ticks, faster
        label ("SPEED", 130);
        static const char* rateNames[engine::kNumArpRates] = { "Slow", "Normal", "Fast", "Very fast" };
        for (int i = 0; i < engine::kNumArpRates; ++i)
        {
            const auto r = rateRect (i).toFloat();
            const bool selected = arp.rate == i;
            drawTile (g, r, selected);
            const auto icon = r.withTrimmedBottom (18.0f).reduced (14.0f, 12.0f);
            const int ticks = 1 << i;
            g.setColour (selected ? theme::onAccent : theme::text.withAlpha (0.85f));
            for (int t = 0; t < ticks; ++t)
                g.fillRoundedRectangle (icon.getX() + icon.getWidth() * ((float) t + 0.5f) / (float) ticks - 1.5f, icon.getY(), 3.0f, icon.getHeight(), 1.5f);
            drawTileLabel (g, r, rateNames[i], selected);
        }

        // The rhythm: a ring with one dot for each step. The lit dots play. Drag the ring to turn it.
        label ("RHYTHM", 206);
        drawRing (g, ringRect(), arp);

        static const char* presetNames[4] = { "Even", "Offbeat", "Gallop", "Spread" };
        for (int i = 0; i < 4; ++i)
            drawPill (g, presetRect (i), presetNames[i], presetMatches (i, arp), false);

        // How many of the steps play, and how long the rhythm is
        drawStepper (g, hitsRect(), "Notes", arp.hits > arp.steps ? arp.steps : arp.hits);
        drawStepper (g, stepsRect(), "Steps", arp.steps);

        // One octave or two
        drawPill (g, octaveRect (0), "1 octave", arp.octaves == 1, false);
        drawPill (g, octaveRect (1), "2 octaves", arp.octaves == 2, false);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto p = e.getPosition();
        const auto arp = processor.getArpSettings();

        if (onRect().contains (p))
            setBool (processor.arpOnParam, ! arp.on);
        for (int i = 0; i < engine::kNumArpStyles; ++i)
            if (styleRect (i).contains (p))
                setChoice (processor.arpStyleParam, i);
        for (int i = 0; i < engine::kNumArpRates; ++i)
            if (rateRect (i).contains (p))
                setChoice (processor.arpRateParam, i);

        static const int presets[4][3] = { { 8, 8, 0 }, { 8, 4, 1 }, { 8, 5, 0 }, { 8, 3, 0 } };
        for (int i = 0; i < 4; ++i)
            if (presetRect (i).contains (p))
            {
                setInt (processor.arpStepsParam, presets[i][0]);
                setInt (processor.arpHitsParam, presets[i][1]);
                setInt (processor.arpRotationParam, presets[i][2]);
            }

        if (hitsRect().contains (p))
            setInt (processor.arpHitsParam, juce::jlimit (1, arp.steps, std::min (arp.hits, arp.steps) + (p.x > hitsRect().getCentreX() ? 1 : -1)));
        if (stepsRect().contains (p))
        {
            const int steps = juce::jlimit (engine::kMinArpSteps, engine::kMaxArpSteps, arp.steps + (p.x > stepsRect().getCentreX() ? 1 : -1));
            setInt (processor.arpStepsParam, steps);
            setInt (processor.arpHitsParam, std::min (std::min (arp.hits, arp.steps), steps));
            setInt (processor.arpRotationParam, arp.rotation % steps);
        }
        for (int i = 0; i < 2; ++i)
            if (octaveRect (i).contains (p))
                setInt (processor.arpOctavesParam, i + 1);

        // The ring: pulling it round turns the rhythm
        if (ringRect().expanded (6).contains (p))
        {
            dragStartAngle = angleAt (p);
            dragStartRotation = arp.rotation;
            turning = true;
        }
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! turning)
            return;
        const auto arp = processor.getArpSettings();
        const float stepAngle = juce::MathConstants<float>::twoPi / (float) arp.steps;
        float delta = angleAt (e.getPosition()) - dragStartAngle;
        while (delta > juce::MathConstants<float>::pi)
            delta -= juce::MathConstants<float>::twoPi;
        while (delta < -juce::MathConstants<float>::pi)
            delta += juce::MathConstants<float>::twoPi;
        const int moved = juce::roundToInt (delta / stepAngle);
        const int rotation = ((dragStartRotation + moved) % arp.steps + arp.steps) % arp.steps;
        setInt (processor.arpRotationParam, rotation);
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override { turning = false; }

private:
    void timerCallback() override { repaint(); }

    // ---- where everything is ----
    juce::Rectangle<int> onRect() const { return { getWidth() - 16 - 72, 14, 72, 26 }; }
    juce::Rectangle<int> styleRect (int i) const { return { 16 + i * 88, 66, 80, 56 }; }
    juce::Rectangle<int> rateRect (int i) const { return { 16 + i * 88, 148, 80, 50 }; }
    juce::Rectangle<int> ringRect() const { return { 20, 232, 112, 112 }; }
    juce::Rectangle<int> presetRect (int i) const { return { 152 + (i % 2) * 106, 230 + (i / 2) * 32, 100, 26 }; }
    juce::Rectangle<int> hitsRect() const { return { 152, 296, 100, 20 }; }
    juce::Rectangle<int> stepsRect() const { return { 258, 296, 100, 20 }; }
    juce::Rectangle<int> octaveRect (int i) const { return { 152 + i * 106, 322, 100, 22 }; }

    bool presetMatches (int i, const engine::ArpSettings& arp) const
    {
        static const int presets[4][3] = { { 8, 8, 0 }, { 8, 4, 1 }, { 8, 5, 0 }, { 8, 3, 0 } };
        return arp.steps == presets[i][0] && std::min (arp.hits, arp.steps) == presets[i][1] && arp.rotation % arp.steps == presets[i][2];
    }

    float angleAt (juce::Point<int> p) const
    {
        const auto c = ringRect().getCentre();
        return std::atan2 ((float) (p.y - c.y), (float) (p.x - c.x));
    }

    // ---- drawing ----
    static void drawTile (juce::Graphics& g, juce::Rectangle<float> r, bool selected)
    {
        theme::drawKey (g, r, selected ? theme::accent : theme::pad, 9.0f, 4.0f, false, false);
    }

    void drawTileLabel (juce::Graphics& g, juce::Rectangle<float> r, const char* text, bool selected) const
    {
        if (auto* l = dynamic_cast<theme::LookAndFeel*> (&getLookAndFeel()))
            g.setFont (l->titleFont (12.0f));
        g.setColour (selected ? theme::onAccent : theme::text);
        g.drawText (text, r.withTrimmedTop (r.getHeight() - 24.0f).withTrimmedBottom (6.0f).toNearestInt(), juce::Justification::centred);
    }

    static void drawStyleIcon (juce::Graphics& g, juce::Rectangle<float> r, int style, bool selected)
    {
        // The height of each note (0 low, 1 high), for a few notes in a row
        static const float up[4] = { 0.0f, 0.34f, 0.67f, 1.0f };
        static const float down[4] = { 1.0f, 0.67f, 0.34f, 0.0f };
        static const float upDown[5] = { 0.0f, 0.5f, 1.0f, 0.5f, 0.0f };
        static const float random[5] = { 0.5f, 0.1f, 0.85f, 0.3f, 0.7f };
        const float* heights = style == 0 ? up : style == 1 ? down : style == 2 ? upDown : random;
        const int count = style < 2 ? 4 : 5;

        juce::Path line;
        for (int i = 0; i < count; ++i)
        {
            const juce::Point<float> at (r.getX() + r.getWidth() * (float) i / (float) (count - 1), r.getBottom() - r.getHeight() * heights[i]);
            if (i == 0)
                line.startNewSubPath (at);
            else
                line.lineTo (at);
        }
        const auto ink = selected ? theme::onAccent : theme::text;
        g.setColour (ink.withAlpha (style == 3 ? 0.0f : 0.45f)); // at random there is no line between the notes
        g.strokePath (line, juce::PathStrokeType (1.6f));
        g.setColour (ink);
        for (int i = 0; i < count; ++i)
            g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ r.getX() + r.getWidth() * (float) i / (float) (count - 1), r.getBottom() - r.getHeight() * heights[i] }));
    }

    void drawPill (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text, bool on, bool big) const
    {
        const auto face = theme::drawKey (g, r.toFloat(), on ? (big ? theme::go : theme::accent) : theme::pad, big ? 12.0f : 8.0f, 3.0f, false, false);
        if (auto* l = dynamic_cast<theme::LookAndFeel*> (&getLookAndFeel()))
            g.setFont (l->titleFont (big ? 14.0f : 12.0f));
        g.setColour (on ? theme::onAccent : theme::text);
        g.drawText (text, face.toNearestInt(), juce::Justification::centred);
    }

    void drawStepper (juce::Graphics& g, juce::Rectangle<int> r, const char* name, int value) const
    {
        g.setColour (theme::pad.darker (0.2f));
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
        g.setColour (theme::text);
        if (auto* l = dynamic_cast<theme::LookAndFeel*> (&getLookAndFeel()))
            g.setFont (l->titleFont (12.0f));
        g.drawText ("-", r.withWidth (22), juce::Justification::centred);
        g.drawText ("+", r.withTrimmedLeft (r.getWidth() - 22), juce::Justification::centred);
        g.drawText (juce::String (name) + " " + juce::String (value), r.reduced (22, 0), juce::Justification::centred);
    }

    void drawRing (juce::Graphics& g, juce::Rectangle<int> area, const engine::ArpSettings& arp) const
    {
        const auto pattern = arp.pattern();
        const auto c = area.toFloat().getCentre();
        const float radius = (float) area.getWidth() * 0.5f - 10.0f;

        g.setColour (theme::pad.darker (0.3f));
        g.fillEllipse (area.toFloat());
        g.setColour (theme::outline);
        g.drawEllipse (area.toFloat().reduced (0.5f), 1.2f);

        const auto dot = [&] (int i)
        {
            const float angle = -juce::MathConstants<float>::halfPi + juce::MathConstants<float>::twoPi * (float) i / (float) pattern.steps;
            return juce::Point<float> (c.x + radius * std::cos (angle), c.y + radius * std::sin (angle));
        };

        // The lit dots are joined: the shape is the rhythm
        juce::Path shape;
        bool first = true;
        for (int i = 0; i < pattern.steps; ++i)
            if (pattern.onset[(size_t) i])
            {
                if (first)
                    shape.startNewSubPath (dot (i));
                else
                    shape.lineTo (dot (i));
                first = false;
            }
        if (! first)
            shape.closeSubPath();
        g.setColour (theme::accent.withAlpha (0.35f));
        g.strokePath (shape, juce::PathStrokeType (1.6f));

        for (int i = 0; i < pattern.steps; ++i)
        {
            const bool lit = pattern.onset[(size_t) i];
            g.setColour (lit ? theme::accent : theme::dimText.withAlpha (0.55f));
            const float size = lit ? 11.0f : 6.0f;
            g.fillEllipse (juce::Rectangle<float> (size, size).withCentre (dot (i)));
            if (i == 0) // the start of the rhythm
            {
                g.setColour (theme::text.withAlpha (0.8f));
                g.drawEllipse (juce::Rectangle<float> (size + 5.0f, size + 5.0f).withCentre (dot (i)), 1.4f);
            }
        }
    }

    // ---- changing the settings ----
    static void setInt (juce::AudioParameterInt* parameter, int value)
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) value));
        parameter->endChangeGesture();
    }
    static void setChoice (juce::AudioParameterChoice* parameter, int index)
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) index));
        parameter->endChangeGesture();
    }
    static void setBool (juce::AudioParameterBool* parameter, bool value)
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (value ? 1.0f : 0.0f);
        parameter->endChangeGesture();
    }

    ProgrezzorProcessor& processor;
    bool turning = false;
    float dragStartAngle = 0.0f;
    int dragStartRotation = 0;
};

// The question card: a small panel in the middle of the bar with the question, and the Replace button on top of it
class ProgressionBar::AskCard : public juce::Component
{
public:
    juce::String text;

    AskCard() { setInterceptsMouseClicks (false, false); }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (3.0f);
        g.setColour (theme::shade.withAlpha (0.35f));
        g.fillRoundedRectangle (r.translated (0.0f, 3.0f), 12.0f);
        g.setColour (theme::panel);
        g.fillRoundedRectangle (r, 12.0f);
        g.setColour (theme::accent.withAlpha (0.85f));
        g.drawRoundedRectangle (r, 12.0f, 2.0f);
        if (auto* l = dynamic_cast<theme::LookAndFeel*> (&getLookAndFeel()))
            g.setFont (l->titleFont (16.0f));
        g.setColour (theme::text);
        g.drawText (text, getLocalBounds().withTrimmedRight (120).withTrimmedLeft (22), juce::Justification::centredLeft);
    }
};

// The hint of Learn mode: a piece of a keyboard (two or three octaves) with the keys to hold lit up, and the keys you hold now
// in green, so that you can see how close you are. Any octave is right, so it is only one way to play the chord.
class ProgressionBar::LearnKeys : public juce::Component
{
public:
    std::vector<int> target, held;
    bool flats = false;
    juce::String caption;

    LearnKeys() { setInterceptsMouseClicks (false, false); }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (3.0f);
        g.setColour (theme::shade.withAlpha (0.35f));
        g.fillRoundedRectangle (r.translated (0.0f, 3.0f), 12.0f);
        g.setColour (theme::panel);
        g.fillRoundedRectangle (r, 12.0f);
        g.setColour (theme::accent.withAlpha (0.85f));
        g.drawRoundedRectangle (r, 12.0f, 2.0f);

        if (target.empty())
            return;

        // Which part of the keyboard: from the C below the lowest key to show, over as many octaves as the chord needs
        const int lowest = *std::min_element (target.begin(), target.end());
        const int highest = *std::max_element (target.begin(), target.end());
        const int first = (lowest / 12) * 12;
        const int octaves = highest >= first + 24 ? 3 : 2;
        const int lastNote = first + 12 * octaves; // the C that ends the piece

        const auto keys = r.reduced (14.0f, 10.0f).withTrimmedTop (16.0f);
        const int whiteCount = 7 * octaves + 1;
        const float whiteWidth = keys.getWidth() / (float) whiteCount;
        const auto isBlack = [] (int note) { const int n = note % 12; return n == 1 || n == 3 || n == 6 || n == 8 || n == 10; };
        const auto contains = [] (const std::vector<int>& list, int note) { return std::find (list.begin(), list.end(), note) != list.end(); };

        // White key positions
        std::vector<std::pair<int, float>> whites; // note, x
        float x = keys.getX();
        for (int note = first; note <= lastNote; ++note)
            if (! isBlack (note))
            {
                whites.push_back ({ note, x });
                x += whiteWidth;
            }
        const auto xOfNote = [&] (int note) // the left edge of a key, a black key halfway over the white one below it
        {
            for (const auto& w : whites)
                if (w.first == note)
                    return w.second;
            for (const auto& w : whites)
                if (w.first == note - 1)
                    return w.second + whiteWidth * 0.7f;
            return keys.getX();
        };

        const auto fill = [&] (int note) -> juce::Colour
        {
            const bool wanted = contains (target, note), down = contains (held, note);
            if (wanted && down)
                return theme::go;
            if (wanted)
                return theme::hotPink;
            if (down)
                return theme::chordColour (7);
            return isBlack (note) ? theme::keyBlack : theme::keyWhite;
        };

        if (auto* l = dynamic_cast<theme::LookAndFeel*> (&getLookAndFeel()))
            g.setFont (l->titleFont (12.0f));
        for (const auto& w : whites)
        {
            const auto key = juce::Rectangle<float> (w.second, keys.getY(), whiteWidth, keys.getHeight());
            g.setColour (fill (w.first));
            g.fillRect (key.reduced (0.6f, 0.0f));
            g.setColour (theme::keyLine);
            g.drawRect (key, 0.8f);
            if (contains (target, w.first)) // the name of a key to hold
            {
                g.setColour (theme::onAccent);
                g.drawText (juce::String (engine::noteName (w.first, flats)), key.withTrimmedTop (key.getHeight() * 0.62f).toNearestInt(), juce::Justification::centred);
            }
        }
        for (int note = first; note <= lastNote; ++note)
            if (isBlack (note))
            {
                const auto key = juce::Rectangle<float> (xOfNote (note), keys.getY(), whiteWidth * 0.62f, keys.getHeight() * 0.6f);
                g.setColour (fill (note));
                g.fillRect (key);
                g.setColour (theme::shade.withAlpha (0.7f));
                g.drawRect (key, 0.8f);
                if (contains (target, note))
                {
                    g.setColour (theme::onAccent);
                    g.drawText (juce::String (engine::noteName (note, flats)), key.toNearestInt(), juce::Justification::centred);
                }
            }

        if (auto* l = dynamic_cast<theme::LookAndFeel*> (&getLookAndFeel()))
            g.setFont (l->labelFont (12.0f));
        g.setColour (theme::dimText);
        g.drawText (caption, r.withHeight (18.0f).reduced (14.0f, 0.0f).translated (0.0f, 2.0f).toNearestInt(), juce::Justification::centredLeft);
    }
};

// Capture: while it is on, every chord you play (on a pad with the mouse, or from the MIDI keyboard) is put at the end of the
// bar as a chord of one bar. It is a switch, so it is clear when it is on. Undo takes the chords away again.
void ProgressionBar::setCapturing (bool on)
{
    capturing = on && editing && ! textMode && ! processor.isLearning();
    captureButton.setToggleState (capturing, juce::dontSendNotification);
    captureSeen = processor.getPlayedCount();
    repaint (trayBounds);
}

void ProgressionBar::toggleCapture()
{
    setCapturing (! capturing);
}

void ProgressionBar::takePlayedChords()
{
    if (! capturing)
        return;
    if (! editing || textMode || processor.isLearning()) // Play, Quick-text and Learn switch it off
    {
        setCapturing (false);
        return;
    }
    const auto played = processor.chordsPlayedSince (captureSeen);
    captureSeen = processor.getPlayedCount();
    for (const auto& chord : played)
        processor.editProgression (engine::withInserted (processor.getProgression(), (int) processor.getProgression().size(), chord, processor.getHostBarBeats()));
    if (! played.empty())
        captureVersion = processor.getProgressionVersion();
}

void ProgressionBar::toggleLearn()
{
    processor.setLearning (! processor.isLearning());
    updateButtons();
}

void ProgressionBar::setLearn (bool active, int target, const std::vector<bool>& done, const std::vector<int>& targetNotes,
                               const std::vector<int>& heldNotes, bool showHint, int total)
{
    const bool hintBefore = learnActive && learnShowHint && learnTarget >= 0;
    learnActive = active;
    learnTarget = target;
    learnDone = done;
    learnNotes = targetNotes;
    learnHeld = heldNotes;
    learnShowHint = showHint;
    learnTotal = total;
    for (int i = 0; i < blocks.size(); ++i)
        blocks[i]->setLearn (active && i < (int) done.size() && done[(size_t) i] && i != target, active && i == target);

    const bool hintNow = active && showHint && target >= 0;
    if (hintNow)
    {
        learnKeys->target = targetNotes;
        learnKeys->held = heldNotes;
        learnKeys->flats = engine::keyUsesFlats (processor.getKey());
        learnKeys->caption = "Hold these keys at the same time (any octave)";
        learnKeys->repaint();
    }
    if (hintNow != hintBefore)
        resized(); // the keyboard takes room from the blocks
    learnKeys->setVisible (hintNow);
    repaint (stripBounds);
    repaint (labelBounds.withWidth (getWidth()));
}

void ProgressionBar::flashLearnWrong()
{
    if (learnTarget >= 0 && learnTarget < blocks.size())
        blocks[learnTarget]->flashWrong();
}

void ProgressionBar::toggleArp()
{
    auto& parameter = *processor.arpOnParam;
    parameter.beginChangeGesture();
    parameter.setValueNotifyingHost (parameter.get() ? 0.0f : 1.0f);
    parameter.endChangeGesture();
    updateButtons();
    if (parameter.get() && ! arpPanelShown) // the first time it is switched on, the choices are shown
        showArpPanel();
}

void ProgressionBar::showArpPanel()
{
    arpPanelShown = true;
    juce::CallOutBox::launchAsynchronously (makeArpPanel(), arpButton.getScreenBounds(), nullptr);
}

std::unique_ptr<juce::Component> ProgressionBar::makeArpPanel()
{
    return std::make_unique<ArpPanel> (processor);
}

ProgressionBar::ProgressionBar (ProgrezzorProcessor& p) : processor (p)
{
    learnKeys = std::make_unique<LearnKeys>();
    addChildComponent (*learnKeys);
    learnButton.setTooltip ("Learn: hold the keys of each chord of the progression on your MIDI keyboard, one chord after the other");
    learnButton.setComponentID ("learn");
    learnButton.onClick = [this] { toggleLearn(); };
    addAndMakeVisible (learnButton);

    captureButton.setToggle();
    captureButton.setRecordDot();
    captureButton.setTooltip ("Rec: while it is on, every chord you play is added to the end of the bar, one bar each. Undo takes them away");
    captureButton.setComponentID ("rec");
    captureButton.onClick = [this] { setCapturing (captureButton.getToggleState()); };
    addAndMakeVisible (captureButton);

    arpButton.setTooltip ("Arp: play everything one note at a time (the MIDI file follows)");
    arpButton.setComponentID ("arp");
    arpButton.onClick = [this] { toggleArp(); };
    addAndMakeVisible (arpButton);
    arpOptionsButton.setTooltip ("How the arp plays: pattern, speed and rhythm");
    arpOptionsButton.setComponentID ("arp-options");
    arpOptionsButton.onClick = [this] { showArpPanel(); };
    addAndMakeVisible (arpOptionsButton);

    playButton.setClickingTogglesState (false);
    playButton.onClick = [this]
    {
        if (processor.isPlayOn() || ! processor.getProgression().empty()) // nothing to play in an empty bar
            processor.togglePlay();
        updateButtons();
    };
    playButton.setTooltip ("Play the progression. Stop it to change the chords. It follows the DAW when the DAW is playing");
    addAndMakeVisible (playButton);

    settingsButton.setTooltip ("Settings: where the chords usually sit (kept for every project)");
    settingsButton.setComponentID ("settings");
    settingsButton.onClick = [this] { showSettingsMenu(); };
    addAndMakeVisible (settingsButton);

    octaveDownButton.setTooltip ("Lower everything by an octave: the pads, the progression and the MIDI file");
    octaveUpButton.setTooltip ("Raise everything by an octave: the pads, the progression and the MIDI file");
    octaveDownButton.setComponentID ("octave-down");
    octaveUpButton.setComponentID ("octave-up");
    octaveDownButton.onClick = [this] { shiftGeneralOctave (-1); };
    octaveUpButton.onClick = [this] { shiftGeneralOctave (1); };
    addAndMakeVisible (octaveDownButton);
    addAndMakeVisible (octaveUpButton);

    askCard = std::make_unique<AskCard>();
    addChildComponent (*askCard);
    replaceButton.setTooltip ("Replace the chords in the bar (Undo brings them back)");
    replaceButton.setVisible (false);
    replaceButton.setAccented (true);
    replaceButton.setComponentID ("replace");
    replaceButton.onClick = [this]
    {
        askingReplace = false;
        processor.suggestProgression();
        updateNotice();
    };
    addChildComponent (replaceButton);

    quickTextButton.setTooltip ("Type the chords, like Am F C G:2. Enter uses it, Esc cancels");
    quickTextButton.setVisible (false);
    quickTextButton.onClick = [this] { startQuickText(); };
    quickTextButton.setComponentID ("quicktext");
    addChildComponent (quickTextButton);

    allowButton.setTooltip ("Turn Lock to scale off, so these chords play");
    allowButton.setVisible (false);
    allowButton.onClick = [this] { processor.allowOutsideScale(); };
    allowButton.setComponentID ("allow");
    allowButton.setAccented (true);
    addChildComponent (allowButton);

    textEditor.setMultiLine (true, true);
    textEditor.setReturnKeyStartsNewLine (false);
    textEditor.setScrollbarsShown (false);
    textEditor.setFont (juce::Font (juce::FontOptions (19.0f)));
    textEditor.setColour (juce::TextEditor::backgroundColourId, theme::field);
    textEditor.setColour (juce::TextEditor::textColourId, theme::text);
    textEditor.setColour (juce::TextEditor::outlineColourId, theme::outline);
    textEditor.setColour (juce::TextEditor::focusedOutlineColourId, theme::accent.withAlpha (0.7f));
    textEditor.setColour (juce::CaretComponent::caretColourId, theme::accent);
    textEditor.setTextToShowWhenEmpty ("Am F C G", theme::dimText);
    textEditor.setComponentID ("quicktext-field");
    textEditor.onTextChange = [this] { updateTextFeedback(); };
    textEditor.onReturnKey = [this] { applyQuickText(); };
    textEditor.onEscapeKey = [this] { cancelQuickText(); };
    addChildComponent (textEditor);
    textMarks = std::make_unique<TextMarks> (textEditor);
    addChildComponent (*textMarks);

    undoButton.setTooltip ("Undo");
    undoButton.onClick = [this]
    {
        cancelQuickText();
        processor.undo();
        clearSelection();
    };
    addAndMakeVisible (undoButton);

    redoButton.setTooltip ("Redo");
    redoButton.onClick = [this]
    {
        cancelQuickText();
        processor.redo();
        clearSelection();
    };
    addAndMakeVisible (redoButton);


    dragTile.makeFile = [this] { return processor.saveMidi(); };
    dragTile.setTooltip ("Drag this into your DAW: the progression as a MIDI file");
    addAndMakeVisible (dragTile);

    updateButtons();
}

ProgressionBar::~ProgressionBar()
{
    // A block held with the mouse must not go on sounding if the window closes
    for (int i = 0; i < blocks.size(); ++i)
        if (blocks[i]->isHeldByMouse())
            processor.releaseBlock (i);
}

void ProgressionBar::setEditing (bool nowEditing)
{
    if (! nowEditing && textMode)
        cancelQuickText();

    editing = nowEditing;
    processor.setEditing (nowEditing);
    if (! nowEditing)
        clearSelection();
    for (auto* block : blocks)
        block->setEditable (nowEditing);
    showDropGap (-1);
    updateVisibility();
    resized();
    repaint();
}

// What is shown: the blocks with + and the pencil (Edit), or the text field (Quick-text)
void ProgressionBar::updateVisibility()
{
    for (auto* block : blocks)
        block->setVisible (! textMode);
    quickTextButton.setVisible (editing && ! textMode);
    textEditor.setVisible (textMode);
    textMarks->setVisible (textMode);
    allowButton.setVisible (! textMode && noticeHasAllow);
}

void ProgressionBar::startQuickText()
{
    if (! editing || textMode)
        return;

    clearSelection();
    textMode = true;
    progressionBeforeText = processor.getProgression();
    textEditor.setText (juce::String (engine::formatQuickText (progressionBeforeText, processor.getKey(), processor.getHostBarBeats())), false);
    updateTextFeedback();
    updateVisibility();
    resized();
    textEditor.grabKeyboardFocus();
    textEditor.moveCaretToEnd();
    repaint();
}

void ProgressionBar::leaveTextMode()
{
    if (! textMode)
        return;

    textMode = false;
    problemRanges.clear();
    updateNotice();
    updateVisibility();
    resized();
    repaint();
    if (auto* parent = getParentComponent())
        parent->grabKeyboardFocus(); // so that Esc and Delete work in the editor again
}

void ProgressionBar::cancelQuickText()
{
    if (! textMode)
        return;

    processor.editProgression (progressionBeforeText); // if Enter used part of the text, it is undone
    leaveTextMode();
}

void ProgressionBar::applyQuickText()
{
    if (! textMode)
        return;

    const auto key = processor.getKey();
    const auto read = engine::parseQuickText (textEditor.getText().toStdString(), key, processor.getHostBarBeats(), processor.isLockedToScale());
    updateTextFeedback();

    // Words that could be read are used, the rest stays marked. If nothing could be read, the bar is left alone.
    if (read.blocks.empty() && ! read.problems.empty())
        return;

    processor.editProgression (engine::keepingBlockState (processor.getProgression(), read.blocks));
    if (read.problems.empty())
        leaveTextMode();
}

void ProgressionBar::updateTextFeedback()
{
    const auto text = textEditor.getText().toStdString();
    const auto read = engine::parseQuickText (text, processor.getKey(), processor.getHostBarBeats(), processor.isLockedToScale());

    problemRanges.clear();
    for (const auto& problem : read.problems)
        problemRanges.add (juce::Range<int> (juce::String::fromUTF8 (text.c_str(), problem.start).length(),
                                             juce::String::fromUTF8 (text.c_str(), problem.end).length()));
    textMarks->ranges = problemRanges;
    textMarks->repaint();

    if (! read.problems.empty())
    {
        noticeText = juce::String (read.problems.front().message);
        if (read.problems.size() > 1)
            noticeText += " (and " + juce::String ((int) read.problems.size() - 1) + " more)";
        noticeIsWarning = true;
    }
    else
    {
        noticeText = read.outsideScale > 0 ? "Not in the scale, so silent: press Enter, then Allow to play" : "Enter uses it, Esc cancels";
        noticeIsWarning = false;
    }
    noticeHasAllow = false;
    allowButton.setVisible (false);
    repaint();
}

// The short message under the blocks: chords that Quick-text put outside the scale are silent until they are allowed
void ProgressionBar::updateNotice()
{
    if (textMode)
        return;

    if (askingReplace)
    {
        const auto asked = noticeText;
        noticeText = "Replace the chords in the bar?";
        noticeIsWarning = false;
        noticeHasAllow = false;
        allowButton.setVisible (false);
        replaceButton.setVisible (true);
        askCard->text = "Replace the chords in the bar?";
        askCard->setVisible (true);
        if (noticeText != asked || noticeBounds.isEmpty())
        {
            resized();
            repaint();
        }
        return;
    }
    replaceButton.setVisible (false);
    askCard->setVisible (false);

    silentCount = 0;
    juce::String firstName;
    const auto key = processor.getKey();
    const auto& progression = processor.getProgression();
    for (int i = 0; i < (int) progression.size(); ++i)
        if (processor.isBlockSilent (i))
        {
            if (silentCount++ == 0)
                firstName = juce::String (engine::chordName (progression[(size_t) i].chord, key));
        }

    const auto before = noticeText;
    noticeHasAllow = silentCount > 0;
    noticeIsWarning = silentCount > 0;
    noticeText = silentCount == 0 ? juce::String()
               : (silentCount == 1 ? firstName + " is not in the scale, so it is silent."
                                   : juce::String (silentCount) + " chords are not in the scale, so they are silent.");
    allowButton.setVisible (noticeHasAllow);
    if (noticeText != before)
    {
        resized();
        repaint();
    }
}

void ProgressionBar::updateButtons()
{
    // Play on: the blocks play their chord when pressed. Play off: the bar can be edited. Follows the button, and the DAW
    // starting and stopping.
    const bool playOn = processor.isPlayOn();
    playButton.setToggleState (playOn, juce::dontSendNotification);
    playButton.setEnabled (playOn || ! processor.getProgression().empty()); // an empty bar cannot be played
    arpButton.setToggleState (processor.getArpSettings().on, juce::dontSendNotification);
    learnButton.setToggleState (processor.isLearning(), juce::dontSendNotification);
    takePlayedChords();
    captureButton.setEnabled (editing && ! textMode && ! processor.isLearning());

    // Which register we are in: Oct 0 is the usual place (around middle C), +1 an octave higher
    const int octave = processor.getGeneralOctave();
    const juce::String label = "Oct " + (octave > 0 ? "+" + juce::String (octave) : juce::String (octave));
    if (label != octaveLabel)
    {
        octaveLabel = label;
        repaint (octaveLabelBounds);
        repaint (stripBounds); // the lit band on the little keyboard moves with it
    }
    octaveDownButton.setEnabled (octave > -engine::kMaxGeneralOctave);
    octaveUpButton.setEnabled (octave < engine::kMaxGeneralOctave);

    // The notes that sound light up on the little keyboard
    std::uint64_t soundingNow[2] = {};
    for (int note = 0; note < 128; ++note)
        if (processor.isNoteSounding (note))
            soundingNow[note / 64] |= (std::uint64_t) 1 << (note % 64);
    if (soundingNow[0] != lastSounding[0] || soundingNow[1] != lastSounding[1])
    {
        lastSounding[0] = soundingNow[0];
        lastSounding[1] = soundingNow[1];
        repaint (stripBounds);
    }
    if (playOn != lastPlayOn)
    {
        lastPlayOn = playOn;
        setEditing (! playOn);
    }
    undoButton.setEnabled (processor.canUndo());
    redoButton.setEnabled (processor.canRedo());
    dragTile.setReady (! processor.getProgression().empty());
    if (blocks.size() == (int) processor.getProgression().size()) // the length follows a resize, and the DAW's time signature
        for (int i = 0; i < blocks.size(); ++i)
            blocks[i]->setLength (processor.getProgression()[(size_t) i].beats, processor.getHostBarBeats());
}

// Suggest would throw away chords the user has made: the bar asks first, in its message line (no dialog box)
// Settings: where the chords usually sit, from two octaves lower to two higher than around middle C. It is kept for every
// project (a file of its own); the Oct buttons move the chords from there, in each project.
void ProgressionBar::chooseHomeOctave (int octaves)
{
    processor.setHomeOctave (octaves);
    repaint (stripBounds);
}

void ProgressionBar::showSettingsMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("The chords usually sit");
    const char* names[5] = { "Much lower  (2 octaves down)", "Lower  (1 octave down)", "Normal  (around middle C)",
                             "Higher  (1 octave up)", "Much higher  (2 octaves up)" };
    for (int i = 0; i < 5; ++i)
        menu.addItem (i + 1, names[i], true, i - 2 == processor.getHomeOctave());
    menu.addSeparator();
    menu.addItem (100, "Kept for every project. The keyboard shows where.", false, false);
    menu.addSeparator();
    menu.addSectionHeader ("Learn");
    menu.addItem (20, "Show hints from the start", true, processor.getLearnHints());

    juce::Component::SafePointer<ProgressionBar> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (settingsButton).withMinimumWidth (260),
                        [safe] (int result)
                        {
                            if (safe != nullptr && result >= 1 && result <= 5)
                                safe->chooseHomeOctave (result - 3);
                            else if (safe != nullptr && result == 20)
                                safe->processor.setLearnHints (! safe->processor.getLearnHints());
                        });
}

// The general register: everything that sounds and is exported, an octave up or down
void ProgressionBar::shiftGeneralOctave (int steps)
{
    auto& parameter = *processor.octaveParam;
    const int wanted = juce::jlimit (-engine::kMaxGeneralOctave, engine::kMaxGeneralOctave, parameter.get() + steps);
    parameter.beginChangeGesture();
    parameter.setValueNotifyingHost (parameter.convertTo0to1 ((float) wanted));
    parameter.endChangeGesture();
    updateButtons();
}

void ProgressionBar::askToReplace (Pending what)
{
    pending = what;
    askingReplace = true;
    askedAtVersion = processor.getProgressionVersion();
    updateNotice();
}

void ProgressionBar::cancelAsk()
{
    if (askingReplace)
    {
        askingReplace = false;
        updateNotice();
    }
}

void ProgressionBar::select (int index)
{
    selected = index == selected ? -1 : index;
    for (int i = 0; i < blocks.size(); ++i)
        blocks[i]->setSelected (i == selected);
}

void ProgressionBar::clearSelection()
{
    selected = -1;
    for (int i = 0; i < blocks.size(); ++i)
        blocks[i]->setSelected (false);
}

// A chord dragged from a pad: over the middle of a block it replaces that block, near an edge or between blocks it is
// inserted. -1 when it is not over the middle of a block.
int ProgressionBar::blockAtScreenPoint (juce::Point<int> screen) const
{
    const auto p = getLocalPoint (nullptr, screen);
    for (size_t i = 0; i < slots.size(); ++i)
        if (slots[i].reduced (slots[i].getWidth() / 5, 0).contains (p))
            return (int) i;
    return -1;
}

void ProgressionBar::setDragActive (bool nowActive)
{
    if (nowActive && ! dragActive && capturing)
    {
        // A pad that is pulled down into the bar was pressed first, and Capture has already taken that press. The drop is the
        // one that counts, so the chord is not in the bar twice: take the one Capture added back.
        takePlayedChords();
        if (processor.getProgressionVersion() == captureVersion && processor.canUndo())
            processor.undo();
    }
    if (nowActive != dragActive)
    {
        dragActive = nowActive;
        repaint();
    }
}

void ProgressionBar::showDropTarget (int block)
{
    for (int i = 0; i < blocks.size(); ++i)
        blocks[i]->setDropTarget (i == block);
}

bool ProgressionBar::replaceBlock (int index, const engine::Chord& chord)
{
    if (index < 0 || index >= (int) processor.getProgression().size())
        return false;

    processor.editProgression (engine::withReplaced (processor.getProgression(), index, chord));
    clearSelection();
    return true;
}

bool ProgressionBar::deleteSelected()
{
    if (selected < 0 || selected >= (int) processor.getProgression().size())
        return false;

    processor.editProgression (engine::withErased (processor.getProgression(), selected));
    clearSelection();
    return true;
}

// The blocks flow in rows, so a point is "after" a block if it is on a lower row, or on the same row and further right
int ProgressionBar::gapAt (juce::Point<int> p) const
{
    int gap = 0;
    for (const auto& slot : slots)
        if (p.y >= slot.getBottom() || (p.y >= slot.getY() && slot.getCentreX() < p.x))
            ++gap;
    return gap;
}

bool ProgressionBar::containsScreenPoint (juce::Point<int> screen) const
{
    return getLocalBounds().contains (getLocalPoint (nullptr, screen));
}

int ProgressionBar::gapAtScreenPoint (juce::Point<int> screen) const
{
    return gapAt (getLocalPoint (nullptr, screen));
}

void ProgressionBar::showDropGap (int gap)
{
    if (gap != dropGap)
    {
        dropGap = gap;
        repaint();
    }
}

void ProgressionBar::refresh()
{
    if (processor.getProgressionVersion() != askedAtVersion)
        askingReplace = false; // anything that changes the bar answers the question
    const auto key = processor.getKey();
    const auto& progression = processor.getProgression();

    // Same number of blocks as before: only the names change, so that a block that is held is not torn down
    if ((int) progression.size() != blocks.size())
    {
        for (int i = 0; i < blocks.size(); ++i)
            if (blocks[i]->isHeldByMouse())
                processor.releaseBlock (i);

        clearSelection();
        blocks.clear();
        for (int i = 0; i < (int) progression.size(); ++i)
        {
            auto* block = blocks.add (new BlockButton());
            block->onPress = [this, i]
            {
                if (i < (int) processor.getProgression().size() && ! processor.isBlockSilent (i))
                    processor.pressBlock (i, processor.getProgression()[(size_t) i].chord);
            };
            block->onRelease = [this, i] { processor.releaseBlock (i); };
            block->onSelect = [this, i] { select (i); };
            block->onToggleLock = [this, i] { processor.editProgression (engine::withLockToggled (processor.getProgression(), i)); };
            block->onOctave = [this, i] (int steps)
            {
                processor.editProgression (engine::withOctaveShift (processor.getProgression(), i, steps));
            };
            block->onDelete = [this, i]
            {
                processor.editProgression (engine::withErased (processor.getProgression(), i));
                clearSelection();
            };

            // The length: the bar keeps its scale while the edge is dragged, so that the edge follows the mouse
            block->onResizeStart = [this, i]
            {
                beforeGesture = processor.getProgression();
                resizing = true;
                resizeStartWidth = i < blocks.size() ? blocks[i]->getWidth() : 0;
            };
            block->onResizeDrag = [this, i] (int dx)
            {
                if (! resizing || i >= (int) beforeGesture.size())
                    return;

                // The edge follows the mouse. The other blocks shrink as this one grows, so the beats are found from the
                // width the mouse asks for.
                const double wanted = (double) (resizeStartWidth + dx);
                double low = engine::kMinBlockBeats, high = engine::kMaxBlockBeats;
                for (int step = 0; step < 40; ++step)
                {
                    const double middle = 0.5 * (low + high);
                    auto trial = beforeGesture;
                    trial[(size_t) i].beats = middle;
                    (middle * scaleFor (trial) < wanted ? low : high) = middle;
                }
                processor.setProgression (engine::withLength (beforeGesture, i, 0.5 * (low + high)));
            };
            block->onResizeEnd = [this]
            {
                // One step for Undo, however long the drag was
                const auto result = processor.getProgression();
                processor.setProgression (beforeGesture);
                processor.editProgression (result);
                resizing = false;
                resized();
            };

            block->onMoveDrag = [this] (juce::Point<int> centre) { showDropGap (gapAt (centre)); };
            block->onMoveEnd = [this, i] (juce::Point<int> centre, bool copy)
            {
                const int gap = gapAt (centre);
                showDropGap (-1);
                if (copy)
                    processor.editProgression (engine::withDuplicated (processor.getProgression(), i, gap));
                else
                    processor.editProgression (engine::withMoved (processor.getProgression(), i, gap));
                clearSelection();
                resized(); // back to its place if nothing changed
            };
            block->setEditable (editing);
            addChildComponent (block);
            block->setVisible (! textMode);
        }
    }

    for (int i = 0; i < blocks.size(); ++i)
    {
        const auto& block = progression[(size_t) i];
        blocks[i]->setChordName (engine::chordName (block.chord, key));
        blocks[i]->setLength (block.beats, processor.getHostBarBeats());
        blocks[i]->setRoot (key.tonic + block.chord.root);
        blocks[i]->setNumeral (engine::romanNumeral (block.chord, key.scale));
        blocks[i]->setLocked (block.locked);
        blocks[i]->setOctave (block.chord.octave);
        blocks[i]->setOutsideScale (processor.isLockedToScale() && ! engine::chordFitsScale (block.chord, key.scale));
        blocks[i]->setSilent (processor.isBlockSilent (i));
        blocks[i]->setPlaying (i == playingBlock);
        blocks[i]->setSelected (i == selected);
    }

    updateNotice();
    updateButtons();
    resized();
    repaint();
}

void ProgressionBar::setPlayingBlock (int index)
{
    if (index == playingBlock)
        return;

    playingBlock = index;
    for (int i = 0; i < blocks.size(); ++i)
        blocks[i]->setPlaying (i == playingBlock);
}

// A little keyboard from the lowest to the highest note the chords can reach, with a lit band where the chords sit now. It
// follows the Oct buttons, so that you see the pitch instead of reading it.
void ProgressionBar::drawPitchStrip (juce::Graphics& g, juce::Rectangle<float> area) const
{
    constexpr int firstNote = 24, lastNote = 96; // C1 to C7: where the chords sit in practice (the band is cut off at the ends)
    const float perNote = area.getWidth() / (float) (lastNote - firstNote);
    const auto xOf = [&] (int note) { return area.getX() + (float) (note - firstNote) * perNote; };

    juce::Path clip;
    clip.addRoundedRectangle (area, 6.0f);
    juce::Graphics::ScopedSaveState state (g);
    g.reduceClipRegion (clip);

    // The keys: white, with the black ones as short dark marks
    g.setColour (theme::keyWhite);
    g.fillRect (area);
    for (int note = firstNote; note < lastNote; ++note)
    {
        const int inOctave = note % 12;
        const bool black = inOctave == 1 || inOctave == 3 || inOctave == 6 || inOctave == 8 || inOctave == 10;
        if (black)
        {
            g.setColour (theme::keyBlack);
            g.fillRect (xOf (note), area.getY(), std::max (1.0f, perNote), area.getHeight() * 0.62f);
        }
        else if (inOctave == 0) // a thin line at every C
        {
            g.setColour (theme::keyLine);
            g.fillRect (xOf (note), area.getY(), 1.0f, area.getHeight());
        }
    }

    // Where the chords sit: the centre of the chords, with room for the notes round it (Settings and the Oct buttons together)
    const int centre = engine::kCentreNote + 12 * processor.getTotalOctave();
    const float left = xOf (juce::jlimit (firstNote, lastNote, centre - 9)), right = xOf (juce::jlimit (firstNote, lastNote, centre + 9));
    g.setColour (theme::accent.withAlpha (0.55f));
    g.fillRect (left, area.getY(), right - left, area.getHeight());
    g.setColour (theme::accent);
    g.drawRect (left, area.getY(), right - left, area.getHeight(), 1.6f);

    // Learn: the notes of the chord to play, when the hint is on
    if (learnActive && learnShowHint)
        for (int note : learnNotes)
            if (note >= firstNote && note < lastNote)
            {
                g.setColour (theme::hotPink);
                g.fillRect (xOf (note), area.getY(), std::max (3.0f, perNote), area.getHeight());
                g.setColour (theme::shade.withAlpha (0.6f));
                g.drawRect (xOf (note), area.getY(), std::max (3.0f, perNote), area.getHeight(), 0.8f);
            }

    // The notes that sound now: lit keys, in the colour of Play
    for (int note = firstNote; note < lastNote; ++note)
        if (processor.isNoteSounding (note))
        {
            g.setColour (theme::go);
            g.fillRect (xOf (note), area.getY(), std::max (2.5f, perNote), area.getHeight());
            g.setColour (theme::shade.withAlpha (0.5f));
            g.drawRect (xOf (note), area.getY(), std::max (2.5f, perNote), area.getHeight(), 0.8f);
        }

    // Middle C: always in the same place, whatever Settings and the Oct buttons say. It is what the rest is measured from.
    const float middleC = xOf (engine::kCentreNote) + perNote * 0.5f;
    g.setColour (theme::chordColour (7));
    g.fillRect (middleC - 1.0f, area.getY(), 2.0f, area.getHeight());
    {
        const juce::String text = "Mid C";
        const float labelWidth = 30.0f, labelHeight = 11.0f;
        const float labelX = juce::jlimit (area.getX(), area.getRight() - labelWidth, middleC - labelWidth * 0.5f);
        const auto label = juce::Rectangle<float> (labelX, area.getBottom() - labelHeight, labelWidth, labelHeight);
        g.setColour (theme::chordColour (7));
        g.fillRoundedRectangle (label, 3.0f);
        g.setColour (theme::onAccent);
        g.setFont (juce::Font (juce::FontOptions (9.5f)).boldened());
        g.drawText (text, label.toNearestInt(), juce::Justification::centred);
    }

    g.setColour (theme::keyEdge);
    g.drawRoundedRectangle (area.reduced (0.5f), 6.0f, 1.2f);
}

void ProgressionBar::paint (juce::Graphics& g)
{
    // A card of its own, lighter than the window, with the blocks in a darker tray inside it. A stripe of colour along the
    // top edge, from the first note to the last.
    const auto card = getLocalBounds().toFloat().reduced (0.5f);
    g.setGradientFill (juce::ColourGradient (theme::cardTop, 0.0f, card.getY(), theme::cardBottom, 0.0f, card.getBottom(), false));
    g.fillRoundedRectangle (card, 14.0f);
    {
        juce::Graphics::ScopedSaveState state (g);
        juce::Path clip;
        clip.addRoundedRectangle (card, 14.0f);
        g.reduceClipRegion (clip);
        juce::ColourGradient rainbow (theme::chordColour (0), card.getX(), 0.0f, theme::chordColour (11), card.getRight(), 0.0f, false);
        for (int pc = 1; pc < 11; ++pc)
            rainbow.addColour ((double) pc / 11.0, theme::chordColour (pc));
        g.setGradientFill (rainbow);
        g.fillRect (card.getX(), card.getY(), card.getWidth(), 4.0f);
    }
    g.setColour (theme::cardEdge);
    g.drawRoundedRectangle (card, 14.0f, 1.6f);
    if (! trayBounds.isEmpty())
    {
        const auto tray = trayBounds.toFloat();
        g.setColour (theme::tray);
        g.fillRoundedRectangle (tray, 10.0f);
        {
            // A faint diagonal weave in the tray
            juce::Graphics::ScopedSaveState state (g);
            juce::Path clip;
            clip.addRoundedRectangle (tray, 10.0f);
            g.reduceClipRegion (clip);
            g.setColour (theme::trayWeave);
            for (float x = tray.getX() - tray.getHeight(); x < tray.getRight(); x += 14.0f)
                g.drawLine (x, tray.getBottom(), x + tray.getHeight(), tray.getY(), 1.0f);
        }
        g.setColour (theme::trayEdge);
        g.drawRoundedRectangle (tray.reduced (0.5f), 10.0f, 1.4f);
    }

    if (blocks.isEmpty())
    {
        g.setColour (theme::dimText);
        if (auto* l = dynamic_cast<theme::LookAndFeel*> (&getLookAndFeel()))
            g.setFont (l->labelFont (16.0f));
        g.drawText (editing ? "Drag a chord down here, or press Suggest" : capturing ? "Rec is on: play a chord and it lands here" : "Press Suggest, or switch Rec on and play",
                    trayBounds, juce::Justification::centred);
    }

    if (noticeText.isNotEmpty() && ! noticeBounds.isEmpty() && ! askingReplace)
    {
        g.setColour (noticeIsWarning ? theme::warn : theme::dimText);
        if (auto* l = dynamic_cast<theme::LookAndFeel*> (&getLookAndFeel()))
            g.setFont (l->labelFont (14.0f));
        g.drawFittedText (noticeText, noticeBounds, juce::Justification::centredLeft, 1, 0.8f);
    }

    // The name of the area, and the ruler: the bar number where each block starts
    if (auto* l = dynamic_cast<theme::LookAndFeel*> (&getLookAndFeel()))
    {
        g.setFont (l->labelFont (13.0f));
        g.setColour (theme::dimText);
        g.setColour (theme::accent);
        g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ (float) labelBounds.getX() + 7.0f, (float) labelBounds.getCentreY() }));
        g.setColour (theme::dimText);
        g.drawText ("PROGRESSION", labelBounds.withTrimmedLeft (16), juce::Justification::centredLeft);

        // Learn: what to play now
        if (learnActive)
        {
            const auto key = processor.getKey();
            juce::String status;
            const int total = learnTotal;
            int done = 0;
            for (bool d : learnDone)
                done += d ? 1 : 0;
            if (learnTarget < 0)
                status = total > 0 ? "All done! Press Learn to go again" : "Nothing to learn yet: put chords in the bar";
            else if (learnTarget < (int) processor.getProgression().size())
                status = "Play " + juce::String (engine::chordName (processor.getProgression()[(size_t) learnTarget].chord, key))
                         + " on your keyboard   (" + juce::String (done + 1) + " of " + juce::String (total) + ")";
            g.setFont (l->titleFont (14.0f));
            g.setColour (learnTarget < 0 ? theme::go : theme::accent);
            g.drawText (status, trayBounds.withHeight (labelBounds.getHeight()).withY (labelBounds.getY()).withTrimmedLeft (130), juce::Justification::centredLeft);
        }

        // A small keyboard, and where the chords sit on it
        if (! stripBounds.isEmpty())
            drawPitchStrip (g, stripBounds.toFloat());

        // The general register, between Oct- and Oct+
        if (! octaveLabelBounds.isEmpty())
        {
            g.setFont (l->titleFont (14.0f));
            g.setColour (processor.getGeneralOctave() != 0 ? theme::accent : theme::text.withAlpha (0.85f));
            g.drawText (octaveLabel, octaveLabelBounds, juce::Justification::centred);
        }

        if (! textMode && slots.size() == (size_t) processor.getProgression().size())
        {
            g.setFont (l->labelFont (11.0f));
            const double barBeats = std::max (1.0, processor.getHostBarBeats());
            double start = 0.0;
            for (size_t i = 0; i < slots.size(); ++i)
            {
                const auto& slot = slots[i];
                const double bar = start / barBeats + 1.0;
                const juce::String number = std::abs (bar - std::round (bar)) < 0.01 ? juce::String ((int) std::round (bar)) : juce::String (bar, 1);
                const auto ruler = juce::Rectangle<int> (slot.getX(), slot.getY() - 13, slot.getWidth(), 12);
                g.setColour (theme::outline.brighter (0.5f));
                g.fillRect (ruler.getX(), ruler.getBottom() - 1, ruler.getWidth(), 1);
                g.fillRect (ruler.getX(), ruler.getY() + 3, 1, ruler.getHeight() - 3);
                g.setColour (theme::dimText);
                g.drawText (number, ruler.withTrimmedLeft (4), juce::Justification::centredLeft);
                start += processor.getProgression()[i].beats;
            }
        }
    }

    // The edge: soft when the bar can be edited, bright while a chord is pulled from a pad (this is where it lands)
    if (dragActive || editing)
    {
        g.setColour (theme::accent.withAlpha (dragActive ? 0.95f : 0.35f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 12.0f, dragActive ? 2.4f : 1.4f);
    }

    // Where a dragged chord would land
    if (dropGap >= 0)
    {
        juce::Rectangle<int> row = trayBounds.reduced (8, 8).withHeight (56).withWidth (4);
        int x = row.getX();
        if (! slots.empty())
        {
            if (dropGap <= 0)
            {
                row = slots.front();
                x = row.getX() - 4;
            }
            else if (dropGap >= (int) slots.size())
            {
                row = slots.back();
                x = row.getRight() + 3;
            }
            else
            {
                const auto& before = slots[(size_t) dropGap - 1];
                const auto& after = slots[(size_t) dropGap];
                if (after.getY() == before.getY())
                {
                    row = after;
                    x = (before.getRight() + after.getX()) / 2;
                }
                else // the first block of a new row: the mark sits in front of it
                {
                    row = after;
                    x = after.getX() - 4;
                }
            }
        }
        g.setColour (theme::accent);
        g.fillRoundedRectangle ((float) x - 1.5f, (float) row.getY(), 3.0f, (float) row.getHeight(), 1.5f);
    }
}

double ProgressionBar::scaleFor (const engine::Progression& p) const
{
    const double barBeats = std::max (1.0, processor.getHostBarBeats());
    const double most = kPixelsPerBar / barBeats, least = kLeastPixelsPerBar / barBeats;
    if (blockAreaWidth <= 0.0 || p.empty())
        return most;
    const double total = std::max (1.0, engine::totalBeats (p));
    const double fit = (blockAreaWidth - 6.0 * (double) p.size()) / total;
    return std::max (least, std::min (most, fit));
}

void ProgressionBar::resized()
{
    auto area = getLocalBounds().reduced (10, 7);
    // The buttons on both sides start at the top of the tray (under the name of the area), and stay a sensible size
    const int labelHeight = 14;
    const int controlsHeight = std::min (area.getHeight() - labelHeight, 150);
    auto left = area.removeFromLeft (64).withTrimmedTop (labelHeight).withHeight (controlsHeight);
    area.removeFromLeft (10);
    auto learnRow = left.removeFromBottom (46);
    left.removeFromBottom (8); // Play's key has a deeper lip, so this gap is 2 more to look the same as the one above Arp
    auto arpRow = left.removeFromBottom (36);
    left.removeFromBottom (6);
    playButton.setBounds (left);
    learnButton.setBounds (learnRow);
    arpOptionsButton.setBounds (arpRow.removeFromRight (16));
    arpRow.removeFromRight (2);
    arpButton.setBounds (arpRow);

    // The right side. The group (the buttons and the Drag tile) grows with the window, and the little keyboard sits in the
    // bottom right corner under all of it, as wide as the group.
    const int groupWidth = juce::jlimit (214, 460, juce::roundToInt ((float) getWidth() * 0.27f));
    auto group = area.removeFromRight (groupWidth).withTrimmedTop (labelHeight).withHeight (controlsHeight);
    area.removeFromRight (10);
    const int rowHeight = (group.getHeight() - 15) / 4;
    stripBounds = group.removeFromBottom (rowHeight);
    group.removeFromBottom (5);
    auto upper = group;
    dragTile.setBounds (upper.removeFromRight (std::min (68, std::max (56, upper.getHeight()))));
    upper.removeFromRight (10);
    auto right = upper;
    auto captureRow = right.removeFromTop (rowHeight);
    settingsButton.setBounds (captureRow.removeFromRight ((right.getWidth() - 12) / 3));
    captureRow.removeFromRight (6);
    captureButton.setBounds (captureRow);
    right.removeFromTop (5);
    auto smallRow = right.removeFromTop (rowHeight);
    const int smallWidth = (smallRow.getWidth() - 12) / 3;
    undoButton.setBounds (smallRow.removeFromLeft (smallWidth));
    smallRow.removeFromLeft (6);
    redoButton.setBounds (smallRow.removeFromLeft (smallWidth));
    smallRow.removeFromLeft (6);
    quickTextButton.setBounds (smallRow);
    right.removeFromTop (5);

    // Oct- and Oct+ with the register between them
    auto octaveRow = right.removeFromTop (rowHeight);
    octaveDownButton.setBounds (octaveRow.removeFromLeft (smallWidth));
    octaveUpButton.setBounds (octaveRow.removeFromRight (smallWidth));
    octaveLabelBounds = octaveRow;

    // What is left in the middle is the tray the blocks sit in, with the name of the area above it
    labelBounds = area.removeFromTop (labelHeight);
    trayBounds = area;

    // A short message under the blocks or the text, when there is one (with the Allow button at its end)
    noticeBounds = {};
    if (askingReplace)
    {
        // The question sits in the middle of the tray, over the blocks
        const juce::Rectangle<int> card = juce::Rectangle<int> (0, 0, 470, 76).withCentre (trayBounds.getCentre());
        askCard->setBounds (card);
        replaceButton.setBounds (juce::Rectangle<int> (card.getRight() - 24 - 100, card.getCentreY() - 19, 100, 38));
        askCard->toFront (false);
        replaceButton.toFront (false);
    }
    else if (noticeText.isNotEmpty())
    {
        auto row = area.removeFromBottom (22);
        area.removeFromBottom (2);
        noticeBounds = row;
        if (noticeHasAllow || askingReplace) // the button follows the message
        {
            float textWidth = 200.0f;
            if (auto* l = dynamic_cast<theme::LookAndFeel*> (&getLookAndFeel()))
                textWidth = (float) juce::GlyphArrangement::getStringWidthInt (l->labelFont (14.0f), noticeText);
            const int taken = std::min (row.getWidth() - 70, (int) textWidth + 12);
            noticeBounds = row.withWidth (std::max (0, taken));
            (askingReplace ? replaceButton : allowButton).setBounds (row.withTrimmedLeft (std::max (0, taken + 4)).withWidth (askingReplace ? 80 : 66));
        }
    }

    area = area.reduced (8, 6); // the blocks sit a little inside the tray

    // Learn: the keyboard with the keys to hold sits at the bottom of the tray, and the blocks have what is left
    if (learnActive && learnShowHint && learnTarget >= 0)
    {
        const auto keysArea = area.removeFromBottom (92);
        area.removeFromBottom (4);
        learnKeys->setBounds (keysArea.withSizeKeepingCentre (std::min (keysArea.getWidth(), 520), keysArea.getHeight()));
    }

    if (textMode)
    {
        textEditor.setBounds (area);
        textMarks->setBounds (area);
        textEditor.setIndents (10, (area.getHeight() - (int) textEditor.getFont().getHeight()) / 3);
        return;
    }

    const int gap = 6;
    const int minBlock = 56;
    const int maxRowHeight = 76;

    const auto& progression = processor.getProgression();
    slots.clear();
    const bool haveBlocks = blocks.size() == (int) progression.size();

    // Blocks (and then + and the text button) flow left to right and wrap to a new row when the line is full
    // A bar is normally the same width however few blocks there are (so a single block does not fill the bar and its edge can
    // be pulled out). When one block is pulled long, all of them shrink to make room, down to a smallest width; after that
    // the rows wrap.
    blockAreaWidth = area.getWidth();
    const double scale = scaleFor (progression);

    struct Item { int width; int row = 0; int x = 0; };
    std::vector<Item> items;
    if (haveBlocks)
        for (const auto& block : progression)
            items.push_back ({ std::min (area.getWidth(), std::max (minBlock, juce::roundToInt (scale * block.beats))) });

    int rows = 1, x = 0;
    for (auto& item : items)
    {
        if (x > 0 && x + item.width > area.getWidth())
        {
            ++rows;
            x = 0;
        }
        item.row = rows - 1;
        item.x = x;
        x += item.width + gap;
    }

    // Each row has a thin ruler above it with the bar number where each block starts
    const int rulerHeight = 14;
    const int rowGap = 3;
    const int blockRowHeight = juce::jlimit (28, maxRowHeight, (area.getHeight() - (rowGap + rulerHeight) * rows + rowGap) / rows);
    const auto place = [&] (const Item& item)
    {
        return juce::Rectangle<int> (area.getX() + item.x, area.getY() + rulerHeight + item.row * (blockRowHeight + rulerHeight + rowGap), item.width, blockRowHeight);
    };

    for (int i = 0; i < (int) items.size(); ++i)
    {
        const auto slot = place (items[(size_t) i]);
        slots.push_back (slot);
        blocks[i]->setBounds (slot);
    }
}

// ---- Editor ------------------------------------------------------------------------------------------------------

// The chord that follows the mouse while it is pulled from a pad down to the bar
class ProgrezzorEditor::DragGhost : public juce::Component
{
public:
    DragGhost (const juce::String& name, int pitchClass) : chordName (name), colour (theme::chordColour (pitchClass))
    {
        setInterceptsMouseClicks (false, false);
        setSize (104, 62);
    }

    void paint (juce::Graphics& g) override
    {
        // The chord, as a small tilted key in its own colour, that follows the mouse
        theme::drawGlow (g, getLocalBounds().toFloat().reduced (8.0f), colour, 8.0f, 10.0f);
        const auto face = theme::drawKey (g, getLocalBounds().toFloat().reduced (3.0f), colour, 10.0f, 5.0f, false, true);
        g.setColour (theme::textOn (colour));
        if (auto* l = dynamic_cast<theme::LookAndFeel*> (&getLookAndFeel()))
            g.setFont (l->titleFont (24.0f));
        g.drawFittedText (chordName, face.toNearestInt().reduced (4), juce::Justification::centred, 1, 0.6f);
    }

private:
    juce::String chordName;
    juce::Colour colour;
};

ProgrezzorEditor::ProgrezzorEditor (ProgrezzorProcessor& p)
    : AudioProcessorEditor (p), processor (p), bar (p)
{
    setLookAndFeel (&look);

    title.setText ("Progrezzor", juce::dontSendNotification);
    title.setFont (look.titleFont (22.0f));
    title.setColour (juce::Label::textColourId, juce::Colours::transparentBlack); // drawn by the editor, with the mark
    addAndMakeVisible (title);

    keyBox.addItemList (processor.keyParam->choices, 1);
    scaleBox.addItemList (processor.scaleParam->choices, 1);
    keyAttachment = std::make_unique<juce::ComboBoxParameterAttachment> (*processor.keyParam, keyBox);
    scaleAttachment = std::make_unique<juce::ComboBoxParameterAttachment> (*processor.scaleParam, scaleBox);
    keyBox.setTooltip ("Key");
    scaleBox.setTooltip ("Scale");
    addAndMakeVisible (keyBox);
    addAndMakeVisible (scaleBox);

    moodBox.addItemList (processor.moodParam->choices, 1);
    voiceBox.addItemList (processor.voiceParam->choices, 1);
    moodAttachment = std::make_unique<juce::ComboBoxParameterAttachment> (*processor.moodParam, moodBox);
    voiceAttachment = std::make_unique<juce::ComboBoxParameterAttachment> (*processor.voiceParam, voiceBox);
    moodBox.setTooltip ("Mood: the feeling of the progressions Suggest gives");
    voiceBox.setTooltip ("Sound: what you hear when you play. Off: only the MIDI (the MIDI is the same)");
    moodBox.setComponentID ("mood");
    voiceBox.setComponentID ("voice");
    addAndMakeVisible (moodBox);
    addAndMakeVisible (voiceBox);

    lockAttachment = std::make_unique<juce::ButtonParameterAttachment> (*processor.lockParam, lockButton);
    lockButton.setTooltip ("Lock to scale: pads, Suggest and the chord menus stay in the scale");
    lockButton.setComponentID ("lock");
    addAndMakeVisible (lockButton);

    suggestButton.onClick = [this]
    {
        bar.cancelQuickText(); // a new progression replaces the text too
        if (processor.suggestWouldOverwrite())
            bar.askToReplace (ProgressionBar::Pending::Suggest);
        else
            processor.suggestProgression();
    };
    suggestButton.setComponentID ("suggest");
    suggestButton.setTooltip ("A whole new progression");
    addAndMakeVisible (suggestButton);

    saveButton.setTooltip ("Save the progression as a MIDI file");
    saveButton.onClick = [this] { saveMidi(); };
    addAndMakeVisible (saveButton);
    folderButton.setTooltip ("Open the folder with the MIDI files");
    folderButton.onClick = []
    {
        const auto folder = ProgrezzorProcessor::getExportFolder();
        folder.createDirectory();
        folder.revealToUser();
    };
    addAndMakeVisible (folderButton);
    addAndMakeVisible (bar);

    for (int i = 0; i < ProgrezzorProcessor::kNumPads; ++i)
    {
        auto& pad = pads[(size_t) i];
        pad.onPress = [this, i] { padPressed (i); };
        pad.onRelease = [this, i] { processor.releasePad (i); };
        pad.onGear = [this, i] { showVariantMenu (i); };

        // Pulled down into the bar: the chord is added where it is let go
        pad.onDragStart = [this, i]
        {
            bar.setDragActive (true);
            ghost = std::make_unique<DragGhost> (engine::chordName (processor.getPadChord (i), processor.getKey()),
                                                 processor.getKey().tonic + processor.getPadChord (i).root);
            addAndMakeVisible (*ghost);
        };
        pad.onDragMove = [this] (juce::Point<int> screen)
        {
            if (ghost != nullptr)
                ghost->setCentrePosition (getLocalPoint (nullptr, screen));
            const int target = bar.containsScreenPoint (screen) ? bar.blockAtScreenPoint (screen) : -1;
            bar.showDropTarget (target); // the block lights up: the chord will replace it
            bar.showDropGap (target < 0 && bar.containsScreenPoint (screen) ? bar.gapAtScreenPoint (screen) : -1);
        };
        pad.onDragEnd = [this, i] (juce::Point<int> screen)
        {
            ghost.reset();
            bar.setDragActive (false);
            bar.showDropGap (-1);
            bar.showDropTarget (-1);
            const int target = bar.containsScreenPoint (screen) ? bar.blockAtScreenPoint (screen) : -1;
            if (target >= 0)
                bar.replaceBlock (target, processor.getPadChord (i));
            else if (bar.containsScreenPoint (screen))
                processor.editProgression (engine::withInserted (processor.getProgression(), bar.gapAtScreenPoint (screen),
                                                                 processor.getPadChord (i), processor.getHostBarBeats()));
        };
        addAndMakeVisible (pad);
    }

    bar.setEditing (! processor.isPlayOn()); // with Play off the bar can be edited
    updateNames();

    setWantsKeyboardFocus (true);
    setResizable (true, true);
    setResizeLimits (850, 400, 1800, 900);
    setSize (1000, 560);
    startTimerHz (30);
}

ProgrezzorEditor::~ProgrezzorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
    processor.setEditing (false); // the loop must not stay silent when the window closes

    // A pad held with the mouse must not go on sounding if the window closes
    for (int i = 0; i < ProgrezzorProcessor::kNumPads; ++i)
        if (pads[(size_t) i].isHeldByMouse())
            processor.releasePad (i);
}

void ProgrezzorEditor::updateNames()
{
    const auto key = processor.getKey();
    shownKey = key.tonic;
    shownScale = (int) key.scale;
    shownVariant = processor.getVariantVersion();
    shownLock = processor.isLockedToScale() ? 1 : 0;

    for (int i = 0; i < ProgrezzorProcessor::kNumPads; ++i)
    {
        pads[(size_t) i].setChordName (engine::chordName (processor.getPadChord (i), key));
        pads[(size_t) i].setRoot (key.tonic + processor.getPadChord (i).root);
        pads[(size_t) i].setNumeral (engine::romanNumeral (processor.getPadChord (i), key.scale));
        pads[(size_t) i].setHasVariant (processor.padHasVariant (i));
    }

    shownProgression = processor.getProgressionVersion();
    bar.refresh(); // the names of the blocks follow the key too
}

void ProgrezzorEditor::timerCallback()
{
    const auto key = processor.getKey();
    if (key.tonic != shownKey || (int) key.scale != shownScale || processor.getProgressionVersion() != shownProgression
        || processor.getVariantVersion() != shownVariant || (processor.isLockedToScale() ? 1 : 0) != shownLock)
        updateNames();

    bar.updateButtons();

    bar.setPlayingBlock (processor.getPlayingBlock());

    const unsigned lit = processor.litPads();
    for (int i = 0; i < ProgrezzorProcessor::kNumPads; ++i)
        pads[(size_t) i].setLit ((lit & (1u << i)) != 0);

    updateSuggestedPads();
    updateLearning();

    for (auto& pad : pads)
        pad.setDragEnabled (processor.isEditing() && ! bar.isInTextMode());
}

// Learn mode: the user holds the keys of the chord on a MIDI keyboard
void ProgrezzorEditor::startLearnSession()
{
    learn.start (processor.getProgression());
    learnVersion = processor.getProgressionVersion();
}

void ProgrezzorEditor::updateLearning()
{
    const bool on = processor.isLearning();
    if (on != wasLearning)
    {
        wasLearning = on;
        if (on)
            startLearnSession();
    }

    if (! on)
    {
        bar.setLearn (false, -1, {}, {}, {}, false, 0);
        return;
    }

    if (processor.getProgressionVersion() != learnVersion) // the bar was changed: start from the first chord again
        startLearnSession();

    const auto& progression = processor.getProgression();

    // The keys that are held now
    unsigned classes = 0;
    std::vector<int> heldNotes;
    for (int note = 0; note < 128; ++note)
        if (processor.isKeyHeld (note))
        {
            classes |= 1u << (note % 12);
            heldNotes.push_back (note);
        }

    if (learn.update (classes, (int) heldNotes.size(), juce::Time::getMillisecondCounterHiRes(), progression, processor.getKey().tonic)
        == engine::LearnSession::Outcome::Wrong)
        bar.flashLearnWrong();

    const int target = learn.target();
    const bool hint = processor.getLearnHints() || learn.needsHint();
    std::vector<bool> done;
    for (int i = 0; i < learn.blockCount(); ++i)
        done.push_back (learn.isPlayed (i));
    std::vector<int> notes;
    if (target >= 0 && target < (int) progression.size())
    {
        const auto voicing = engine::voiceChord (progression[(size_t) target].chord, processor.getKey().tonic);
        for (int i = 0; i < voicing.count; ++i)
            notes.push_back (voicing.notes[(size_t) i]);
    }
    bar.setLearn (true, target, done, notes, heldNotes, hint, learn.blockCount());

    if (target >= 0)
        bar.repaint(); // the target pulses: keep it moving
}

// The pads that fit well after the last chord: the one played last on a pad, or else the last block in the bar
void ProgrezzorEditor::updateSuggestedPads()
{
    // Paused for now (Henrik, after the test of milestone 7): the hint was not clear. The table stays, see BACKLOG.md.
    constexpr bool kShowSuggestedPads = false;
    if (! kShowSuggestedPads)
    {
        for (auto& pad : pads)
            pad.setSuggested (false);
        return;
    }

    engine::NextPads next;
    const auto played = processor.recentChords (1);
    if (! played.empty())
        next = engine::nextPadsAfter (played.back(), processor.getKey().scale);
    else if (! processor.getProgression().empty())
        next = engine::nextPadsAfter (processor.getProgression().back().chord, processor.getKey().scale);

    for (int i = 0; i < ProgrezzorProcessor::kNumPads; ++i)
    {
        bool glow = false;
        for (int n = 0; n < next.count; ++n)
            glow = glow || next.pads[(size_t) n] == i;
        pads[(size_t) i].setSuggested (glow);
    }
}

// Save MIDI: the file is written, and the button says so for a moment
void ProgrezzorEditor::saveMidi()
{
    const auto file = processor.saveMidi();
    saveButton.setButtonText (file.existsAsFile() ? "Saved" : "Nothing to save");
    saveButton.setTooltip (file.existsAsFile() ? file.getFullPathName() : "Fill the bar first");

    juce::Component::SafePointer<ProgrezzorEditor> safe (this);
    juce::Timer::callAfterDelay (1600, [safe]
    {
        if (safe != nullptr)
        {
            safe->saveButton.setButtonText ("Save MIDI");
            safe->saveButton.setTooltip ("Save the progression as a MIDI file");
        }
    });
}

// A pad is pressed with the mouse: it sounds. (To replace a block, drag the pad onto it.)
void ProgrezzorEditor::padPressed (int pad)
{
    processor.pressPad (pad);
}

// The gear on a pad: the other kinds of chord on the same root, and the inversions. While Lock to scale is on only those
// that fit the scale are listed (and the one the pad plays now); with the lock off all of them are.
void ProgrezzorEditor::showVariantMenu (int pad)
{
    const auto key = processor.getKey();
    const auto base = engine::padChord (key.scale, pad);
    const auto current = processor.getPadChord (pad);

    juce::PopupMenu menu;
    for (int t = 0; t < engine::kNumChordTypes; ++t)
    {
        const engine::Chord chord { base.root, (engine::ChordType) t, engine::kAutoInversion };
        if (processor.isLockedToScale() && chord.type != base.type && chord.type != current.type
            && ! engine::chordFitsScale (chord, key.scale))
            continue;

        const bool ticked = current.type == chord.type && (current.inversion == engine::kAutoInversion);
        menu.addItem (100 + t, juce::String (engine::chordName (chord, key)), true, ticked);
    }

    const int notes = engine::shapeOf (current.type).count;
    if (notes > 1)
    {
        menu.addSeparator();
        for (int k = 1; k < notes; ++k)
        {
            const engine::Chord chord { base.root, current.type, k };
            menu.addItem (200 + k, juce::String (engine::chordName (chord, key)), true, current.inversion == k);
        }
    }

    juce::Component::SafePointer<ProgrezzorEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&pads[(size_t) pad]),
                        [safe, pad, base, current] (int result)
                        {
                            if (safe == nullptr || result <= 0)
                                return;

                            auto& proc = safe->processor;
                            if (result >= 200)
                                proc.setPadVariant (pad, current.type, result - 200);
                            else if ((engine::ChordType) (result - 100) == base.type)
                                proc.clearPadVariant (pad); // back to the pad's own chord
                            else
                                proc.setPadVariant (pad, (engine::ChordType) (result - 100), engine::kAutoInversion);
                        });
}

bool ProgrezzorEditor::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    const int code = key.getKeyCode();

    if (bar.isInTextMode()) // the text field does not have the focus after a pad was clicked: Enter and Esc still work
    {
        if (code == juce::KeyPress::returnKey)
            bar.applyQuickText();
        else if (code == juce::KeyPress::escapeKey)
            bar.cancelQuickText();
        return code == juce::KeyPress::returnKey || code == juce::KeyPress::escapeKey;
    }

    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey)
        return bar.isEditing() && bar.deleteSelected();

    if (code == juce::KeyPress::escapeKey && bar.getSelected() >= 0)
    {
        bar.clearSelection();
        return true;
    }
    if (code == juce::KeyPress::escapeKey)
        bar.cancelAsk();

    if (mods.isCommandDown() && (code == 'Z' || code == 'z'))
    {
        if (mods.isShiftDown())
            processor.redo();
        else
            processor.undo();
        bar.clearSelection();
        return true;
    }

    if (mods.isCommandDown() && (code == 'Y' || code == 'y'))
    {
        processor.redo();
        bar.clearSelection();
        return true;
    }

    return false;
}

void ProgrezzorEditor::paint (juce::Graphics& g)
{
    const auto all = getLocalBounds().toFloat();

    // A deep indigo backdrop with two soft colour washes and a faint grid of dots
    g.setGradientFill (juce::ColourGradient (theme::backgroundTop, 0.0f, 0.0f, theme::background, 0.0f, all.getHeight(), false));
    g.fillAll();
    g.setGradientFill (juce::ColourGradient (theme::chordColour (9).withAlpha (0.20f * theme::washStrength), all.getWidth() * 0.12f, -20.0f,
                                             theme::chordColour (9).withAlpha (0.0f), all.getWidth() * 0.12f, all.getHeight() * 0.55f, true));
    g.fillRect (all);
    g.setGradientFill (juce::ColourGradient (theme::chordColour (0).withAlpha (0.12f * theme::washStrength), all.getWidth() * 0.95f, all.getHeight() * 0.35f,
                                             theme::chordColour (0).withAlpha (0.0f), all.getWidth() * 0.55f, all.getHeight() * 0.35f, true));
    g.fillRect (all);
    g.setColour (theme::backdropDot);
    for (float y = 10.0f; y < all.getHeight(); y += 22.0f)
        for (float x = 10.0f; x < all.getWidth(); x += 22.0f)
            g.fillRect (x, y, 1.6f, 1.6f);

    if (auto* l = dynamic_cast<theme::LookAndFeel*> (&getLookAndFeel()))
    {
        // The mark: four bars of different heights in four chord colours, and the name over a gradient
        if (title.isVisible())
        {
            const auto area = title.getBounds().toFloat();
            const float barWidth = 8.0f, markHeight = 34.0f;
            const auto nameFont = l->titleFont (30.0f);
            // The bars stand on the same line as the letters of the name
            const float x0 = area.getX() + 2.0f, base = area.getCentreY() - nameFont.getHeight() * 0.5f + nameFont.getAscent();
            const float heights[4] = { 0.55f, 0.8f, 0.65f, 1.0f };
            const int roots[4] = { 9, 5, 0, 7 };
            for (int i = 0; i < 4; ++i)
            {
                const float h = markHeight * heights[i];
                g.setColour (theme::chordColour (roots[i]));
                g.fillRoundedRectangle (x0 + (float) i * (barWidth + 2.5f), base - h, barWidth, h, 3.2f);
            }
            g.setGradientFill (juce::ColourGradient (theme::accent, area.getX() + 52.0f, 0.0f, theme::hotPink, area.getRight(), 0.0f, false));
            g.setFont (l->titleFont (30.0f));
            if (area.getWidth() >= 200.0f)
                g.drawText ("Progrezzor", area.withTrimmedLeft (52.0f).toNearestInt(), juce::Justification::centredLeft);
        }

        g.setFont (l->labelFont (13.0f));
        g.setColour (theme::accent);
        g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ (float) padLabel.getX() + 7.0f, (float) padLabel.getCentreY() }));
        g.setColour (theme::dimText);
        g.drawText ("CHORDS", padLabel.withTrimmedLeft (16), juce::Justification::centredLeft);
    }
}

void ProgrezzorEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    auto top = area.removeFromTop (38);
    // The right side first; the title gives way if the window is narrow
    suggestButton.setBounds (top.removeFromRight (132));
    top.removeFromRight (12);
    folderButton.setBounds (top.removeFromRight (90));
    top.removeFromRight (6);
    saveButton.setBounds (top.removeFromRight (80));
    top.removeFromRight (8);

    const int leftOfTitle = 8 + 76 + 6 + 96 + 6 + 38 + 6 + 104 + 6 + 80;
    const int titleWidth = top.getWidth() - leftOfTitle;
    title.setVisible (titleWidth >= 40);
    if (titleWidth >= 40)
    {
        title.setBounds (top.removeFromLeft (std::min (230, titleWidth)));
        top.removeFromLeft (8);
    }
    keyBox.setBounds (top.removeFromLeft (76));
    top.removeFromLeft (6);
    scaleBox.setBounds (top.removeFromLeft (96));
    top.removeFromLeft (6);
    lockButton.setBounds (top.removeFromLeft (38));
    top.removeFromLeft (6);
    moodBox.setBounds (top.removeFromLeft (104));
    top.removeFromLeft (6);
    voiceBox.setBounds (top.removeFromLeft (80));

    area.removeFromTop (8);
    padLabel = area.removeFromTop (18);

    // The pads are a quarter lower than before (65 % became 49 %), same width; the bar under them gets the rest, with room for several rows
    const int oldBarHeight = juce::jlimit (70, 130, getHeight() / 5);
    const int padHeight = juce::roundToInt ((float) (area.getHeight() - oldBarHeight - 12) * 0.49f);
    bar.setBounds (area.removeFromBottom (area.getHeight() - padHeight - 12));
    area.removeFromBottom (12);

    // Seven pads side by side, over the whole width. A pad draws its key 5 px inside its bounds, so the bounds reach 5 px
    // further out: then the keys themselves start and end exactly at the edges of the area, in line with the Suggest button
    // and the card of the bar below (any odd pixels are spread over the gaps, not left at the right edge).
    const int gap = 10;
    const float keyWidth = (float) (area.getWidth() - gap * (ProgrezzorProcessor::kNumPads - 1)) / (float) ProgrezzorProcessor::kNumPads;
    for (int i = 0; i < ProgrezzorProcessor::kNumPads; ++i)
    {
        const int left = area.getX() + juce::roundToInt ((float) i * (keyWidth + (float) gap));
        const int right = area.getX() + juce::roundToInt ((float) i * (keyWidth + (float) gap) + keyWidth);
        pads[(size_t) i].setBounds (left - 5, area.getY(), right - left + 10, area.getHeight());
    }
}
