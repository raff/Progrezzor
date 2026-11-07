#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "BinaryData.h"

// The dark theme, adapted from SeqVerse's (same palette idea and fonts, with a warm accent for Progrezzor). Everything
// a control looks like comes from here, so the editor code only lays things out.
namespace theme
{
inline const juce::Colour background (0xff0e0c1b);   // deep indigo-black
inline const juce::Colour panel (0xff1a1731);
inline const juce::Colour outline (0xff35305a);
inline const juce::Colour field (0xff211d3c);
inline const juce::Colour pad (0xff2f2b4d);        // a neutral key at rest
inline const juce::Colour padHover (0xff3c3763);
inline const juce::Colour text (0xfff4f1ff);
inline const juce::Colour dimText (0xff9b97bd);
inline const juce::Colour accent (0xffffb347);     // what is lit
inline const juce::Colour hotPink (0xffff5fa8);        // the other end of the Suggest gradient
inline const juce::Colour go (0xff3fd98a);         // Play is on
inline const juce::Colour onAccent (0xff1b1118);   // text on the bright colours
inline const juce::Colour warn (0xffff6b5e);       // a block that is outside the scale
inline const juce::Colour block (0xff3a3358);      // (the blocks of the progression take the colour of their chord)
inline const juce::Colour blockHover (0xff48406c);
inline const juce::Colour blockEdge (0xff5a5080);

// Every chord has its own colour, from its root note, so that F is the same colour on a pad, in the bar and in any key.
inline juce::Colour chordColour (int pitchClass)
{
    static const juce::uint32 palette[12] = { 0xffff5d6c, 0xffff7a3d, 0xffffa629, 0xffffd23f, 0xffb9e03a, 0xff4fd36b,
                                              0xff25c9a5, 0xff2fb8ec, 0xff4a8dff, 0xff8e6bff, 0xffd261ff, 0xffff5fa8 };
    return juce::Colour (palette[((pitchClass % 12) + 12) % 12]);
}

// Black or near-black text on bright faces, white on dark ones
inline juce::Colour textOn (juce::Colour face)
{
    return face.getPerceivedBrightness() > 0.62f ? juce::Colour (0xff1b1118) : juce::Colour (0xfffbf9ff);
}

// A chunky key: a face with a darker lip under it, a thin bevel (light along the top, dark along the bottom), a soft gloss
// on the upper half and a layered shadow. Pressed, the face sinks onto the lip. Gives the face, where the text goes.
inline juce::Rectangle<float> drawKey (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour base, float corner,
                                       float lip, bool pressed, bool hover = false)
{
    r = r.reduced (0.5f);
    const float shadow = 2.0f;
    const auto body = r.withTrimmedBottom (shadow);
    const float sink = pressed ? std::min (lip * 0.65f, 3.0f) : 0.0f; // pressed: a small, quiet dip

    // Shadow under the key, in two soft layers
    g.setColour (juce::Colours::black.withAlpha (0.08f));
    g.fillRoundedRectangle (body.translated (0.0f, shadow).expanded (0.6f, 0.0f), corner + 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.14f));
    g.fillRoundedRectangle (body.translated (0.0f, shadow * 0.5f), corner);

    // The lip, with a dark outline round the whole key
    g.setGradientFill (juce::ColourGradient (base.darker (0.28f), 0.0f, body.getY(), base.darker (0.50f), 0.0f, body.getBottom(), false));
    g.fillRoundedRectangle (body, corner);

    // The face
    const auto face = body.withHeight (body.getHeight() - lip).translated (0.0f, sink);
    const auto top = hover ? base.brighter (0.30f) : base.brighter (0.20f);
    const auto bottom = hover ? base.brighter (0.02f) : base.darker (0.14f);
    g.setGradientFill (juce::ColourGradient (top, 0.0f, face.getY(), bottom, 0.0f, face.getBottom(), false));
    g.fillRoundedRectangle (face, corner);

    // A soft gloss over the upper half
    {
        juce::Graphics::ScopedSaveState state (g);
        juce::Path clip;
        clip.addRoundedRectangle (face, corner);
        g.reduceClipRegion (clip);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.20f), 0.0f, face.getY(),
                                                 juce::Colours::white.withAlpha (0.0f), 0.0f, face.getY() + face.getHeight() * 0.55f, false));
        g.fillRect (face);
    }

    // The bevel: light along the top inside the edge, dark along the bottom
    g.setColour (juce::Colours::white.withAlpha (0.22f));
    g.drawRoundedRectangle (face.reduced (1.0f), corner - 1.0f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.22f));
    g.drawLine (face.getX() + corner * 0.6f, face.getBottom() - 1.5f, face.getRight() - corner * 0.6f, face.getBottom() - 1.5f, 1.0f);

    // The outline
    g.setColour (base.darker (0.7f).withAlpha (0.7f));
    g.drawRoundedRectangle (body, corner, 1.0f);
    g.setColour (base.darker (0.7f).withAlpha (0.7f));
    g.drawRoundedRectangle (face, corner, 1.0f);
    return face;
}

// A soft glow round a rectangle (a pad that plays, a block that is playing)
inline void drawGlow (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour colour, float radius, float corner, float alpha = 0.75f)
{
    juce::Path path;
    path.addRoundedRectangle (r, corner);
    juce::DropShadow (colour.withAlpha (alpha), (int) radius, { 0, 0 }).drawForPath (g, path);
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        labelFace = juce::Typeface::createSystemTypefaceFor (BinaryData::ChakraPetchSemiBold_ttf, BinaryData::ChakraPetchSemiBold_ttfSize);
        titleFace = juce::Typeface::createSystemTypefaceFor (BinaryData::ChakraPetchBold_ttf, BinaryData::ChakraPetchBold_ttfSize);

        setColourScheme ({ background, field, panel, outline, text, accent, onAccent, accent, text });
        setColour (juce::ResizableWindow::backgroundColourId, background);
        setColour (juce::Label::textColourId, dimText);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedTextColourId, text);
    }

    juce::Font labelFont (float height) const { return juce::Font (juce::FontOptions (labelFace).withHeight (height)); }
    juce::Font titleFont (float height) const { return juce::Font (juce::FontOptions (titleFace).withHeight (height)); }

    juce::Font getLabelFont (juce::Label& label) override { return label.getFont(); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return labelFont (15.0f); }
    juce::Font getPopupMenuFont() override { return labelFont (15.0f); }

    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
    {
        const auto bounds = juce::Rectangle<int> (width, height).toFloat();
        const bool isHot = box.isEnabled() && (box.isMouseOver (true) || box.isPopupActive());

        drawKey (g, bounds, isHot ? padHover : pad, 9.0f, 4.0f, false, false);

        const float cx = (float) width - 15.0f, cy = (float) (height - 4) * 0.5f;
        juce::Path chevron;
        chevron.startNewSubPath (cx - 4.5f, cy - 2.0f);
        chevron.lineTo (cx, cy + 2.5f);
        chevron.lineTo (cx + 4.5f, cy - 2.0f);
        g.setColour (isHot ? accent : dimText);
        g.strokePath (chevron, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // The panel that opens from the Arp button: the same dark card as the rest, with a warm edge
    void drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics& g, const juce::Path& path, juce::Image&) override
    {
        g.setColour (juce::Colour (0xff1c1838));
        g.fillPath (path);
        g.setColour (accent.withAlpha (0.7f));
        g.strokePath (path, juce::PathStrokeType (2.0f));
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (11, 1, box.getWidth() - 32, box.getHeight() - 6);
        label.setFont (getComboBoxFont (box));
    }

    void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override
    {
        g.fillAll (panel);
        g.setColour (accent.withAlpha (0.55f));
        g.drawRect (0, 0, width, height, 2);
    }

    void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                            bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& itemText,
                            const juce::String& shortcutKeyText, const juce::Drawable* icon, const juce::Colour* textColour) override
    {
        if (isSeparator || icon != nullptr || shortcutKeyText.isNotEmpty())
        {
            LookAndFeel_V4::drawPopupMenuItem (g, area, isSeparator, isActive, isHighlighted, isTicked, hasSubMenu, itemText,
                                               shortcutKeyText, icon, textColour);
            return;
        }

        auto r = area.reduced (3, 1);
        if (isHighlighted && isActive)
        {
            g.setColour (accent.withAlpha (0.28f));
            g.fillRoundedRectangle (r.toFloat(), 5.0f);
        }
        if (isTicked)
        {
            g.setColour (accent);
            g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ (float) r.getX() + 10.0f, (float) r.getCentreY() }));
        }
        g.setColour (isActive ? (isTicked ? accent : text) : dimText);
        g.setFont (getPopupMenuFont());
        g.drawFittedText (itemText, r.withTrimmedLeft (20).withTrimmedRight (6), juce::Justification::centredLeft, 1);
    }

private:
    juce::Typeface::Ptr labelFace, titleFace;
};
}
