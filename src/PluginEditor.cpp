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
    const float lip = juce::jlimit (8.0f, 18.0f, bounds.getHeight() * 0.07f);
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
    g.setColour (on ? juce::Colours::white.withAlpha (0.9f) : base.darker (0.5f));
    g.fillEllipse (light);
    g.setColour (theme::textOn (base).withAlpha (0.22f));
    for (int dot = -2; dot <= 2; ++dot)
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ face.getCentreX() + (float) dot * 12.0f, face.getBottom() - 15.0f }));

    const float height = juce::jlimit (16.0f, 64.0f, std::min ((float) getHeight() * 0.24f, (float) getWidth() * 0.28f));
    if (auto* l = dynamic_cast<theme::LookAndFeel*> (&getLookAndFeel()))
        g.setFont (l->titleFont (height));
    const auto textArea = face.reduced (6.0f, 10.0f).toNearestInt();
    const auto ink = theme::textOn (base);
    g.setColour (juce::Colours::black.withAlpha (ink.getBrightness() > 0.5f ? 0.28f : 0.0f));
    g.drawFittedText (chordName, textArea.translated (0, 2), juce::Justification::centred, 1, 0.6f);
    g.setColour (ink);
    g.drawFittedText (chordName, textArea, juce::Justification::centred, 1, 0.6f);

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
    g.setColour (juce::Colours::white.withAlpha (0.28f));
    g.fillRoundedRectangle (face.getX() + 8.0f, face.getY() + 2.0f, face.getWidth() - 16.0f, 2.2f, 1.1f);

    // A die: a white rounded square with five dots
    const float size = std::min (face.getHeight() - 10.0f, 26.0f);
    const juce::Rectangle<float> die (face.getX() + 14.0f, face.getCentreY() - size * 0.5f, size, size);
    g.setColour (juce::Colours::white);
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
    const auto face = theme::drawKey (g, getLocalBounds().toFloat(), on ? theme::accent : theme::pad, 10.0f, 4.0f, isButtonDown, isMouseOver);
    drawLock (g, face.withSizeKeepingCentre (face.getHeight() * 0.58f, face.getHeight() * 0.58f),
              on ? theme::onAccent : theme::dimText, on);
}

// ---- PlayToggle ----------------------------------------------------------------------------------------------------

void PlayToggle::paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown)
{
    const bool on = getToggleState();
    const auto bounds = getLocalBounds().toFloat();
    if (on)
        theme::drawGlow (g, bounds.reduced (5.0f, 6.0f), theme::go, 7.0f, 14.0f, 0.3f);
    const auto face = theme::drawKey (g, bounds.reduced (3.0f, 2.0f), on ? theme::go : theme::pad, 14.0f, 6.0f, isButtonDown, isMouseOver);

    // The icon says what a click does: a triangle (start) while it is stopped, a square (stop) while it plays
    const float size = std::min (face.getWidth(), face.getHeight()) * 0.36f;
    g.setColour (on ? theme::onAccent : theme::dimText);
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

// ---- BarButton -----------------------------------------------------------------------------------------------------

void BarButton::paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown)
{
    const bool enabled = isEnabled();
    const auto base = accented ? theme::accent : (isMouseOver && enabled ? theme::padHover : theme::pad);
    const auto face = theme::drawKey (g, getLocalBounds().toFloat(), enabled ? base : theme::pad.darker (0.3f), 9.0f, 4.0f, isButtonDown && enabled, isMouseOver && enabled);
    const auto bounds = face;

    const auto colour = accented ? theme::onAccent
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
                const auto labelArea = bounds.toNearestInt();
                g.setColour (juce::Colours::black.withAlpha (accented ? 0.0f : 0.35f));
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
    const auto colour = ready ? juce::Colours::white : theme::dimText.withAlpha (0.45f);
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
        g.setColour (juce::Colours::white);
        g.drawRoundedRectangle (body.expanded (0.5f), 9.5f, 3.0f);
        const auto light = juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ bounds.getRight() - 12.0f, bounds.getY() + 12.0f });
        g.setColour (theme::go);
        g.fillEllipse (light);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawEllipse (light, 1.0f);
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

        // How long it lasts, right under the name, with how far the chord is moved (when it is) beside it
        const auto lengthFont = l->labelFont (lengthSize);
        const auto row = juce::Rectangle<float> (bounds.getX(), top + nameHeight, bounds.getWidth(), lengthHeight).toNearestInt();
        const float lengthWidth = juce::GlyphArrangement::getStringWidth (lengthFont, lengthLabel);
        const juce::Colour lengthInk = lit ? theme::onAccent.withAlpha (0.85f) : ink.withAlpha (0.72f);
        if (octave == 0)
        {
            g.setFont (lengthFont);
            g.setColour (lengthInk);
            g.drawText (lengthLabel, row, juce::Justification::centred);
        }
        else
        {
            auto mark = octaveMark (octave);
            if (lengthWidth + 10.0f + juce::GlyphArrangement::getStringWidth (lengthFont, mark) > bounds.getWidth() - 8.0f)
                mark = mark.upToFirstOccurrenceOf (" ", false, false); // short of room: just +1
            const float markWidth = juce::GlyphArrangement::getStringWidth (lengthFont, mark);
            const float startX = (float) row.getCentreX() - (lengthWidth + 10.0f + markWidth) * 0.5f;
            g.setFont (lengthFont);
            g.setColour (lengthInk);
            g.drawText (lengthLabel, juce::Rectangle<float> (startX, (float) row.getY(), lengthWidth + 2.0f, (float) row.getHeight()).toNearestInt(), juce::Justification::centredLeft);
            g.setColour (lit ? theme::onAccent : theme::accent);
            g.setFont (l->titleFont (lengthSize));
            g.drawText (mark, juce::Rectangle<float> (startX + lengthWidth + 10.0f, (float) row.getY(), markWidth + 4.0f, (float) row.getHeight()).toNearestInt(), juce::Justification::centredLeft);
        }
    }

    if (silent) // crossed out: it keeps its place and its time, but makes no sound
    {
        g.setColour (theme::warn.withAlpha (0.8f));
        g.drawLine (10.0f, (float) getHeight() - 10.0f, (float) getWidth() - 10.0f, 10.0f, 1.6f);
    }

    // In Edit the lock shows on mouseover, and always on a block that is locked (in Play only on a locked one)
    if (locked || (editable && hovered()))
        drawLock (g, lockArea().toFloat().reduced (5.0f), locked ? (heldByMouse ? theme::onAccent : theme::accent) : theme::dimText, locked);

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

ProgressionBar::ProgressionBar (ProgrezzorProcessor& p) : processor (p)
{
    playButton.setClickingTogglesState (false);
    playButton.onClick = [this] { processor.togglePlay(); updateButtons(); };
    playButton.setTooltip ("Play the progression. Stop it to change the chords. It follows the DAW when the DAW is playing");
    addAndMakeVisible (playButton);

    settingsButton.setTooltip ("Settings: how high or low the chords sit");
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

    replaceButton.setTooltip ("Replace the chords with a new suggestion (Undo brings them back)");
    replaceButton.setVisible (false);
    replaceButton.setAccented (true);
    replaceButton.setComponentID ("replace");
    replaceButton.onClick = [this]
    {
        askingReplace = false;
        processor.suggestProgression();
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

    captureButton.setButtonText ("Capture");
    captureButton.setTooltip ("Put the last 8 chords you played in the bar");
    captureButton.onClick = [this] { captureLastChords(); };
    addAndMakeVisible (captureButton);

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
        if (noticeText != asked || noticeBounds.isEmpty())
        {
            resized();
            repaint();
        }
        return;
    }
    replaceButton.setVisible (false);

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

    // Which register we are in: Oct 0 is the usual place (around middle C), +1 an octave higher
    const int octave = processor.getGeneralOctave();
    const juce::String label = "Oct " + (octave > 0 ? "+" + juce::String (octave) : juce::String (octave));
    static const char* registerNames[engine::kNumRegisters] = { "Low", "Mid", "High" };
    const juce::String registerName = registerNames[juce::jlimit (0, engine::kNumRegisters - 1, processor.getRegisterIndex())];
    if (label != octaveLabel || registerName != registerLabel)
    {
        octaveLabel = label;
        registerLabel = registerName;
        repaint (octaveLabelBounds);
    }
    octaveDownButton.setEnabled (octave > -engine::kMaxGeneralOctave);
    octaveUpButton.setEnabled (octave < engine::kMaxGeneralOctave);
    if (playOn != lastPlayOn)
    {
        lastPlayOn = playOn;
        setEditing (! playOn);
    }
    undoButton.setEnabled (processor.canUndo());
    redoButton.setEnabled (processor.canRedo());
    captureButton.setEnabled (! processor.recentChords (1).empty());
    dragTile.setReady (! processor.getProgression().empty());
    if (blocks.size() == (int) processor.getProgression().size()) // the length follows a resize, and the DAW's time signature
        for (int i = 0; i < blocks.size(); ++i)
            blocks[i]->setLength (processor.getProgression()[(size_t) i].beats, processor.getHostBarBeats());
}

void ProgressionBar::captureLastChords()
{
    // One click: the last 8 chords you played go in the bar (Undo brings the old bar back)
    cancelQuickText();
    if (processor.capture (8))
        clearSelection();
}

// Suggest would throw away chords the user has made: the bar asks first, in its message line (no dialog box)
// Settings: where the chords sit. Low, Mid (the usual) or High, an octave apart. Oct- and Oct+ move it further from there.
void ProgressionBar::chooseRegister (int index)
{
    auto& parameter = *processor.registerParam;
    parameter.beginChangeGesture();
    parameter.setValueNotifyingHost (parameter.convertTo0to1 ((float) juce::jlimit (0, engine::kNumRegisters - 1, index)));
    parameter.endChangeGesture();
    updateButtons();
}

void ProgressionBar::showSettingsMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("Pitch");
    const char* names[engine::kNumRegisters] = { "Low  (deeper, warmer)", "Mid  (the usual)", "High  (brighter)" };
    for (int i = 0; i < engine::kNumRegisters; ++i)
        menu.addItem (i + 1, names[i], true, i == processor.getRegisterIndex());

    juce::Component::SafePointer<ProgressionBar> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (settingsButton).withMinimumWidth (190),
                        [safe] (int result)
                        {
                            if (safe != nullptr && result > 0)
                                safe->chooseRegister (result - 1);
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

void ProgressionBar::askToReplace()
{
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
// follows Settings (Low, Mid, High) and the Oct buttons, so that you see the pitch instead of reading it.
void ProgressionBar::drawPitchStrip (juce::Graphics& g, juce::Rectangle<float> area) const
{
    constexpr int firstNote = 12, lastNote = 108; // C0 to C8: the whole way the Oct buttons can take the chords
    const float perNote = area.getWidth() / (float) (lastNote - firstNote);
    const auto xOf = [&] (int note) { return area.getX() + (float) (note - firstNote) * perNote; };

    juce::Path clip;
    clip.addRoundedRectangle (area, 6.0f);
    juce::Graphics::ScopedSaveState state (g);
    g.reduceClipRegion (clip);

    // The keys: white, with the black ones as short dark marks
    g.setColour (juce::Colour (0xffd9d4ee));
    g.fillRect (area);
    for (int note = firstNote; note < lastNote; ++note)
    {
        const int inOctave = note % 12;
        const bool black = inOctave == 1 || inOctave == 3 || inOctave == 6 || inOctave == 8 || inOctave == 10;
        if (black)
        {
            g.setColour (juce::Colour (0xff2a2545));
            g.fillRect (xOf (note), area.getY(), std::max (1.0f, perNote), area.getHeight() * 0.62f);
        }
        else if (inOctave == 0) // a thin line at every C
        {
            g.setColour (juce::Colour (0xff8f8aa8));
            g.fillRect (xOf (note), area.getY(), 1.0f, area.getHeight());
        }
    }

    // Where the chords sit: the centre of the chords, with room for the notes round it
    const int centre = engine::registerCentre (processor.getRegisterIndex()) + 12 * processor.getGeneralOctave();
    const float left = xOf (juce::jlimit (firstNote, lastNote, centre - 9)), right = xOf (juce::jlimit (firstNote, lastNote, centre + 9));
    g.setColour (theme::accent.withAlpha (0.55f));
    g.fillRect (left, area.getY(), right - left, area.getHeight());
    g.setColour (theme::accent);
    g.drawRect (left, area.getY(), right - left, area.getHeight(), 1.6f);

    g.setColour (juce::Colour (0xff3a3560));
    g.drawRoundedRectangle (area.reduced (0.5f), 6.0f, 1.2f);
}

void ProgressionBar::paint (juce::Graphics& g)
{
    // A card of its own, lighter than the window, with the blocks in a darker tray inside it. A stripe of colour along the
    // top edge, from the first note to the last.
    const auto card = getLocalBounds().toFloat().reduced (0.5f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff25214a), 0.0f, card.getY(), juce::Colour (0xff1a1731), 0.0f, card.getBottom(), false));
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
    g.setColour (juce::Colour (0xff3d376a));
    g.drawRoundedRectangle (card, 14.0f, 1.6f);
    if (! trayBounds.isEmpty())
    {
        const auto tray = trayBounds.toFloat();
        g.setColour (juce::Colour (0xff0d0b1a));
        g.fillRoundedRectangle (tray, 10.0f);
        {
            // A faint diagonal weave in the tray
            juce::Graphics::ScopedSaveState state (g);
            juce::Path clip;
            clip.addRoundedRectangle (tray, 10.0f);
            g.reduceClipRegion (clip);
            g.setColour (juce::Colours::white.withAlpha (0.025f));
            for (float x = tray.getX() - tray.getHeight(); x < tray.getRight(); x += 14.0f)
                g.drawLine (x, tray.getBottom(), x + tray.getHeight(), tray.getY(), 1.0f);
        }
        g.setColour (juce::Colour (0xff2a2550));
        g.drawRoundedRectangle (tray.reduced (0.5f), 10.0f, 1.4f);
    }

    if (blocks.isEmpty())
    {
        g.setColour (theme::dimText);
        if (auto* l = dynamic_cast<theme::LookAndFeel*> (&getLookAndFeel()))
            g.setFont (l->labelFont (16.0f));
        g.drawText (editing ? "Drag a chord down here, or press Suggest" : "Press Suggest, or play and press Capture",
                    trayBounds, juce::Justification::centred);
    }

    if (noticeText.isNotEmpty() && ! noticeBounds.isEmpty())
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

        // A small keyboard, and where the chords sit on it
        if (! stripBounds.isEmpty())
            drawPitchStrip (g, stripBounds.toFloat());

        // The general register, between Oct- and Oct+
        if (! octaveLabelBounds.isEmpty())
        {
            // Two lines: the default register from Settings (Low, Mid, High), and the Oct- and Oct+ steps from there
            auto textArea = octaveLabelBounds;
            g.setFont (l->labelFont (12.0f));
            g.setColour (theme::dimText);
            g.drawText (registerLabel, textArea.removeFromTop (textArea.getHeight() / 2 + 1), juce::Justification::centredBottom);
            g.setFont (l->titleFont (14.0f));
            g.setColour (processor.getGeneralOctave() != 0 ? theme::accent : theme::text.withAlpha (0.85f));
            g.drawText (octaveLabel, textArea, juce::Justification::centredTop);
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
    playButton.setBounds (left);

    // The right side: the Drag tile, and Capture on top of Undo, Redo and the pencil (which stays in its place)
    dragTile.setBounds (area.removeFromRight (std::min (68, std::max (56, area.getHeight()))).withTrimmedTop (labelHeight).withHeight (controlsHeight));
    area.removeFromRight (10);
    auto right = area.removeFromRight (112).withTrimmedTop (labelHeight).withHeight (controlsHeight);
    area.removeFromRight (10);
    const int rowHeight = (right.getHeight() - 15) / 4;
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

    // A small keyboard that shows where the chords sit
    stripBounds = right.removeFromTop (rowHeight);
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
    if (noticeText.isNotEmpty())
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
            bar.askToReplace();
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

    for (auto& pad : pads)
        pad.setDragEnabled (processor.isEditing() && ! bar.isInTextMode());
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
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff17122e), 0.0f, 0.0f, theme::background, 0.0f, all.getHeight(), false));
    g.fillAll();
    g.setGradientFill (juce::ColourGradient (theme::chordColour (9).withAlpha (0.20f), all.getWidth() * 0.12f, -20.0f,
                                             theme::chordColour (9).withAlpha (0.0f), all.getWidth() * 0.12f, all.getHeight() * 0.55f, true));
    g.fillRect (all);
    g.setGradientFill (juce::ColourGradient (theme::chordColour (0).withAlpha (0.12f), all.getWidth() * 0.95f, all.getHeight() * 0.35f,
                                             theme::chordColour (0).withAlpha (0.0f), all.getWidth() * 0.55f, all.getHeight() * 0.35f, true));
    g.fillRect (all);
    g.setColour (juce::Colours::white.withAlpha (0.045f));
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

    // The pads are a third lower than before (65 %); the bar under them gets the rest, with room for several rows
    const int oldBarHeight = juce::jlimit (70, 130, getHeight() / 5);
    const int padHeight = juce::roundToInt ((float) (area.getHeight() - oldBarHeight - 12) * 0.65f);
    bar.setBounds (area.removeFromBottom (area.getHeight() - padHeight - 12));
    area.removeFromBottom (12);

    // Seven pads side by side, over the whole width
    const int gap = 10;
    const int padWidth = (area.getWidth() - gap * (ProgrezzorProcessor::kNumPads - 1)) / ProgrezzorProcessor::kNumPads;
    for (int i = 0; i < ProgrezzorProcessor::kNumPads; ++i)
        pads[(size_t) i].setBounds (area.getX() + i * (padWidth + gap), area.getY(), padWidth, area.getHeight());
}
