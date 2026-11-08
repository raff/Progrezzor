#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "BinaryData.h"

// The dark theme, adapted from SeqVerse's (same palette idea and fonts, with a warm accent for Progrezzor). Everything
// a control looks like comes from here, so the editor code only lays things out.
namespace theme
{
// Every colour of the editor is here: the editor's own code names no colour, it only asks for these. apply() sets them for a
// look, and the values they start with are Midnight's.
inline juce::Colour background (0xff0e0c1b);
inline juce::Colour backgroundTop (0xff17122e);   // the top of the backdrop, which fades into `background`
inline juce::Colour panel (0xff1a1731);
inline juce::Colour outline (0xff35305a);
inline juce::Colour field (0xff211d3c);
inline juce::Colour pad (0xff2f2b4d);              // a neutral key at rest
inline juce::Colour padHover (0xff3c3763);
inline juce::Colour text (0xfff4f1ff);
inline juce::Colour dimText (0xff9b97bd);
inline juce::Colour accent (0xffffb347);           // what is lit
inline juce::Colour hotPink (0xffff5fa8);          // the other end of the Suggest gradient
inline juce::Colour go (0xff3fd98a);               // Play is on, and the notes that sound
inline juce::Colour onAccent (0xff1b1118);         // text on the bright colours
inline juce::Colour warn (0xffff6b5e);             // a block that is outside the scale
inline juce::Colour block (0xff3a3358);
inline juce::Colour blockHover (0xff48406c);
inline juce::Colour blockEdge (0xff5a5080);
inline juce::Colour cardTop (0xff25214a);          // the card of the progression bar
inline juce::Colour cardBottom (0xff1a1731);
inline juce::Colour cardEdge (0xff3d376a);
inline juce::Colour tray (0xff0d0b1a);             // the tray the blocks sit in
inline juce::Colour trayEdge (0xff2a2550);
inline juce::Colour trayWeave (0x06ffffff);        // the faint diagonal lines in the tray
inline juce::Colour keyWhite (0xffd9d4ee);         // the little keyboard
inline juce::Colour keyBlack (0xff2a2545);
inline juce::Colour keyLine (0xff8f8aa8);
inline juce::Colour keyEdge (0xff3a3560);
inline juce::Colour popup (0xff1c1838);            // menus and the Arp panel
inline juce::Colour backdropDot (0x0bffffff);      // the grid of dots on the backdrop
inline float washStrength = 1.0f;                  // how strong the soft colour washes on the backdrop are
inline float chordSaturation = 1.0f;               // how colourful the chord colours are (0 is grey)
inline float chordBrightness = 1.0f;

// The two looks that are defined. Midnight is the one in use. There is no switch for it: another look is only a reason to
// change apply() below (a switch in Settings is in BACKLOG.md, for if users ask for it).
constexpr int kNumThemes = 2;
constexpr const char* themeNames[kNumThemes] = { "Midnight", "Syre" };

inline const juce::Colour shine = juce::Colours::white; // highlights and light marks, in every look
inline const juce::Colour shade = juce::Colours::black; // shadows and dark outlines, in every look

inline void apply (int index)
{
    const auto c = [] (juce::uint32 argb) { return juce::Colour (argb); };
    switch (index)
    {
        case 1: // Syre: the look of SubForge and SeqVerse, graphite with teal, and softer chord colours
            background = c (0xff0b0d0c); backgroundTop = c (0xff141716); panel = c (0xff141716); outline = c (0xff232826);
            field = c (0xff1c201e); pad = c (0xff29302c); padHover = c (0xff343c38);
            text = c (0xffe8ede6); dimText = c (0xff8a938d); accent = c (0xff3dd6c6); hotPink = c (0xff7ce38b); go = c (0xff5be37a);
            onAccent = c (0xff0b0d0c); warn = c (0xffff6b5e);
            block = c (0xff2a312d); blockHover = c (0xff353d38); blockEdge = c (0xff46514b);
            cardTop = c (0xff1c211f); cardBottom = c (0xff141716); cardEdge = c (0xff2f3835);
            tray = c (0xff090b0a); trayEdge = c (0xff232826); trayWeave = c (0x05ffffff);
            keyWhite = c (0xffdfe6e0); keyBlack = c (0xff27302c); keyLine = c (0xff8a948e); keyEdge = c (0xff2f3835);
            popup = c (0xff161a18); backdropDot = c (0x08ffffff);
            washStrength = 0.35f; chordSaturation = 0.62f; chordBrightness = 0.97f;
            break;



        default: // Midnight: deep indigo and bright chord colours
            background = c (0xff0e0c1b); backgroundTop = c (0xff17122e); panel = c (0xff1a1731); outline = c (0xff35305a);
            field = c (0xff211d3c); pad = c (0xff2f2b4d); padHover = c (0xff3c3763);
            text = c (0xfff4f1ff); dimText = c (0xff9b97bd); accent = c (0xffffb347); hotPink = c (0xffff5fa8); go = c (0xff3fd98a);
            onAccent = c (0xff1b1118); warn = c (0xffff6b5e);
            block = c (0xff3a3358); blockHover = c (0xff48406c); blockEdge = c (0xff5a5080);
            cardTop = c (0xff25214a); cardBottom = c (0xff1a1731); cardEdge = c (0xff3d376a);
            tray = c (0xff0d0b1a); trayEdge = c (0xff2a2550); trayWeave = c (0x06ffffff);
            keyWhite = c (0xffd9d4ee); keyBlack = c (0xff2a2545); keyLine = c (0xff8f8aa8); keyEdge = c (0xff3a3560);
            popup = c (0xff1c1838); backdropDot = c (0x0bffffff);
            washStrength = 1.0f; chordSaturation = 1.0f; chordBrightness = 1.0f;
            break;
    }
}

// Every chord has its own colour, from its root note, so that F is the same colour on a pad, in the bar and in any key.
// The look decides how colourful they are.
inline juce::Colour chordColour (int pitchClass)
{
    static const juce::uint32 palette[12] = { 0xffff5d6c, 0xffff7a3d, 0xffffa629, 0xffffd23f, 0xffb9e03a, 0xff4fd36b,
                                              0xff25c9a5, 0xff2fb8ec, 0xff4a8dff, 0xff8e6bff, 0xffd261ff, 0xffff5fa8 };
    return juce::Colour (palette[((pitchClass % 12) + 12) % 12]).withMultipliedSaturation (chordSaturation).withMultipliedBrightness (chordBrightness);
}

// Black or near-black text on bright faces, white on dark ones
inline juce::Colour textOn (juce::Colour face)
{
    return face.getPerceivedBrightness() > 0.62f ? juce::Colour (0xff1b1118) : juce::Colour (0xfffbf9ff);
}

// Text inside a button, a tile or a block: always at least `margin` pixels from the edges. If it does not fit, it is squeezed a
// little (down to 70 %) rather than touching the edge. Whatever the font is, the text never ends up against the border.
inline void drawFitted (juce::Graphics& g, const juce::String& string, juce::Rectangle<int> area,
                        juce::Justification justification = juce::Justification::centred, int margin = 8)
{
    g.drawFittedText (string, area.reduced (margin, 0), justification, 1, 0.7f);
}

// A chunky key: a face with a darker lip under it, a thin bevel (light along the top, dark along the bottom), a soft gloss
// on the upper half and a layered shadow. Pressed, the face sinks onto the lip. Gives the face, where the text goes.
inline juce::Rectangle<float> drawKey (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour base, float corner,
                                       float lip, bool pressed, bool hover = false)
{
    r = r.reduced (0.5f);
    const float shadow = 1.5f;
    const auto body = r.withTrimmedBottom (shadow);
    const float sink = pressed ? std::min (lip * 0.65f, 3.0f) : 0.0f; // pressed: a small, quiet dip

    // Shadow under the key, in two soft layers
    g.setColour (juce::Colours::black.withAlpha (0.03f));
    g.fillRoundedRectangle (body.translated (0.0f, shadow).expanded (0.6f, 0.0f), corner + 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.05f));
    g.fillRoundedRectangle (body.translated (0.0f, shadow * 0.5f), corner);

    // The lip, with a dark outline round the whole key
    g.setGradientFill (juce::ColourGradient (base.darker (0.15f), 0.0f, body.getY(), base.darker (0.30f), 0.0f, body.getBottom(), false));
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
    g.setColour (juce::Colours::black.withAlpha (0.08f));
    g.drawLine (face.getX() + corner * 0.6f, face.getBottom() - 1.5f, face.getRight() - corner * 0.6f, face.getBottom() - 1.5f, 1.0f);

    // The outline
    g.setColour (base.darker (0.6f).withAlpha (0.35f));
    g.drawRoundedRectangle (body, corner, 1.0f);
    g.setColour (base.darker (0.6f).withAlpha (0.35f));
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

        smallFace = juce::Typeface::createSystemTypefaceFor (BinaryData::InterSemiBold_otf, BinaryData::InterSemiBold_otfSize);

        refreshColours();
    }

    // Takes the colours of the look in use now (see theme::apply)
    void refreshColours()
    {
        setColourScheme ({ background, field, panel, outline, text, accent, onAccent, accent, text });
        setColour (juce::ResizableWindow::backgroundColourId, background);
        setColour (juce::Label::textColourId, dimText);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedTextColourId, text);
        setColour (juce::TooltipWindow::backgroundColourId, popup);
        setColour (juce::TooltipWindow::textColourId, text);
        setColour (juce::TooltipWindow::outlineColourId, outline);
    }

    // Chakra Petch is a display face, with cut corners that go jagged on the screen when the text is small. So it is only used for
    // the big names (the logo, the chords, Suggest, from 17 px). Everything smaller is Inter SemiBold, made for small text on screens.
    static constexpr float kDisplayFrom = 17.0f;
    juce::Font labelFont (float height) const { return juce::Font (juce::FontOptions (smallFace).withHeight (height)); }
    juce::Font titleFont (float height) const
    {
        return juce::Font (juce::FontOptions (height >= kDisplayFrom ? titleFace : smallFace).withHeight (height));
    }

    juce::Font getLabelFont (juce::Label& label) override { return label.getFont(); }
    // Tooltips: narrow, so the text takes several lines, and well away from the mouse so that it is plain that it is a tooltip
    static constexpr float kTooltipTextWidth = 210.0f;

    juce::TextLayout tooltipLayout (const juce::String& tip, juce::Colour colour) const
    {
        juce::AttributedString string;
        string.setJustification (juce::Justification::topLeft);
        string.append (tip, labelFont (17.0f), colour);
        string.setWordWrap (juce::AttributedString::byWord);
        juce::TextLayout layout;
        layout.createLayout (string, kTooltipTextWidth);
        return layout;
    }

    juce::Rectangle<int> getTooltipBounds (const juce::String& tip, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override
    {
        const auto layout = tooltipLayout (tip, text);
        const int w = (int) std::ceil (layout.getWidth()) + 30, h = (int) std::ceil (layout.getHeight()) + 24;
        const int gapX = 26, gapY = 30; // clear of the mouse pointer
        const int x = screenPos.x + gapX + w > parentArea.getRight() ? screenPos.x - gapX - w : screenPos.x + gapX;
        const int y = screenPos.y + gapY + h > parentArea.getBottom() ? screenPos.y - gapY - h : screenPos.y + gapY;
        return juce::Rectangle<int> (x, y, w, h).constrainedWithin (parentArea);
    }

    void drawTooltip (juce::Graphics& g, const juce::String& tip, int width, int height) override
    {
        const auto bounds = juce::Rectangle<float> ((float) width, (float) height);
        g.fillAll (popup);
        g.setColour (accent.withAlpha (0.85f));
        g.drawRect (bounds, 1.5f);
        g.setColour (accent);
        g.fillRect (bounds.withWidth (4.0f)); // a coloured edge, so that it stands out from what is behind it
        tooltipLayout (tip, text).draw (g, bounds.reduced (17.0f, 12.0f));
    }

    // A small loudspeaker: with sound waves when it is on, crossed out when it is off
    static void drawSpeaker (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour colour, bool sound)
    {
        const float h = r.getHeight(), x = r.getX(), cy = r.getCentreY();
        juce::Path body;
        body.addRectangle (x, cy - h * 0.17f, h * 0.22f, h * 0.34f);
        body.startNewSubPath (x + h * 0.22f, cy - h * 0.17f);
        body.lineTo (x + h * 0.55f, cy - h * 0.42f);
        body.lineTo (x + h * 0.55f, cy + h * 0.42f);
        body.lineTo (x + h * 0.22f, cy + h * 0.17f);
        body.closeSubPath();
        g.setColour (colour);
        g.fillPath (body);
        if (sound)
        {
            for (int i = 0; i < 2; ++i)
            {
                const float radius = h * (0.3f + 0.22f * (float) i);
                juce::Path wave; // an arc on the right of the cone (angles go clockwise from straight up)
                wave.addCentredArc (x + h * 0.55f, cy, radius, radius, 0.0f, juce::MathConstants<float>::halfPi - 0.75f, juce::MathConstants<float>::halfPi + 0.75f, true);
                g.strokePath (wave, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
        }
        else
        {
            g.setColour (warn);
            g.drawLine (x - h * 0.05f, cy + h * 0.46f, x + h * 0.78f, cy - h * 0.46f, 1.8f);
        }
    }

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

        if (box.getComponentID() == "voice") // the sound chooser: a loudspeaker, crossed out when the sound is Off
        {
            const bool off = box.getSelectedItemIndex() == box.getNumItems() - 1;
            drawSpeaker (g, { 9.0f, (float) (height - 4) * 0.5f - 8.0f, 18.0f, 16.0f }, off ? dimText.withAlpha (0.7f) : text, ! off);
        }
    }

    // The panel that opens from the Arp button: the same dark card as the rest, with a warm edge
    void drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics& g, const juce::Path& path, juce::Image&) override
    {
        g.setColour (popup);
        g.fillPath (path);
        g.setColour (accent.withAlpha (0.7f));
        g.strokePath (path, juce::PathStrokeType (2.0f));
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        const int left = box.getComponentID() == "voice" ? 32 : 11; // room for the loudspeaker
        label.setBounds (left, 1, box.getWidth() - left - 21, box.getHeight() - 6);
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
    juce::Typeface::Ptr labelFace, titleFace, smallFace;
};
}
