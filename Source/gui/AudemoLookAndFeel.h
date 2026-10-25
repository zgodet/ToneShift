/*
    ToneShift by Audemo
    Vintage hardware styling: dark brushed faceplate, ivory knobs, lamp buttons,
    bat-handle toggle switch.
*/
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace audemo
{

namespace colours
{
    inline const juce::Colour plateLight { 0xff3a3b3e }; // gunmetal
    inline const juce::Colour plateDark  { 0xff1c1d1f };
    inline const juce::Colour charcoal   { 0xff121110 };
    inline const juce::Colour charcoalHi { 0xff262422 };
    inline const juce::Colour engraving  { 0xffd8ceb0 }; // cream silkscreen on the plate
    inline const juce::Colour engraveHi  { 0xcc000000 }; // its drop shadow
    inline const juce::Colour ink        { 0xff1e1c19 }; // dark print on ivory/VU faces
    inline const juce::Colour cream      { 0xffe0d3b1 }; // aged ivory
    inline const juce::Colour creamShade { 0xffa3946d };
    inline const juce::Colour amber      { 0xffffb547 };
    inline const juce::Colour amberDim   { 0xff5a3a10 };
    inline const juce::Colour glass      { 0xff17120c };
    inline const juce::Colour lampRed    { 0xffe0402c };
    inline const juce::Colour silkCream  { 0xffe9e0c6 }; // print on charcoal
}

/** Silkscreened legend: cream glyphs with a soft shadow on the dark plate. */
inline void drawEngravedText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                              juce::Justification just, float size, bool bold = true)
{
    g.setFont (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
    g.setColour (colours::engraveHi);
    g.drawText (text, area.translated (0.0f, 1.0f), just, false);
    g.setColour (colours::engraving);
    g.drawText (text, area, just, false);
}

inline void drawScrew (juce::Graphics& g, juce::Point<float> c, float r, float angle)
{
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillEllipse (juce::Rectangle<float> (r * 2.0f + 2.0f, r * 2.0f + 2.0f).withCentre (c.translated (0.0f, 1.0f)));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffa4a6aa), c.x - r, c.y - r,
                                             juce::Colour (0xff3a3c40), c.x + r, c.y + r, false));
    g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
    g.setColour (juce::Colour (0xff1c1d1f));
    const auto d = juce::Point<float> (std::cos (angle), std::sin (angle)) * (r * 0.75f);
    g.drawLine ({ c - d, c + d }, 1.6f);
}

class AudemoLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    AudemoLookAndFeel()
    {
        setColour (juce::TooltipWindow::backgroundColourId, colours::charcoal);
        setColour (juce::TooltipWindow::textColourId, colours::silkCream);
        setColour (juce::TooltipWindow::outlineColourId, colours::charcoalHi);
    }

    // ---- Cream knob with a printed scale ------------------------------------
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        using namespace colours;
        const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        const auto c = bounds.getCentre();
        const float outer = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 2.0f;
        const bool large = s.getProperties().getWithDefault ("large", false);
        const int ticks = s.getProperties().getWithDefault ("ticks", 11);
        const int majorEvery = juce::jmax (1, (int) s.getProperties().getWithDefault ("majorEvery", (ticks - 1) / 2));

        // Scale printed on the faceplate
        const float tickOuter = outer;
        const float tickInner = outer - (large ? 10.0f : 7.0f);
        for (int i = 0; i < ticks; ++i)
        {
            const float a = startAngle + (endAngle - startAngle) * (float) i / (float) (ticks - 1);
            const bool major = i % majorEvery == 0;
            const float inner = major ? tickInner - 3.0f : tickInner;
            g.setColour (engraving.withAlpha (major ? 0.95f : 0.7f));
            g.drawLine ({ c.getPointOnCircumference (inner, a), c.getPointOnCircumference (tickOuter, a) },
                        major ? 1.8f : 1.1f);
        }

        // Skirt
        const float skirtR = tickInner - (large ? 8.0f : 5.0f);
        const auto skirt = juce::Rectangle<float> (skirtR * 2.0f, skirtR * 2.0f).withCentre (c);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillEllipse (skirt.translated (0.0f, 3.0f).expanded (1.0f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3b3a37), c.x, skirt.getY(),
                                                 juce::Colour (0xff141412), c.x, skirt.getBottom(), false));
        g.fillEllipse (skirt);

        // Thin metal rim so the black skirt reads against the dark plate
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8a877f), c.x, skirt.getY(),
                                                 juce::Colour (0xff2e2d2a), c.x, skirt.getBottom(), false));
        g.drawEllipse (skirt.reduced (0.6f), 1.3f);

        // Skirt indicator line
        const float a = startAngle + pos * (endAngle - startAngle);
        g.setColour (silkCream);
        g.drawLine ({ c.getPointOnCircumference (skirtR * 0.80f, a), c.getPointOnCircumference (skirtR - 1.5f, a) }, large ? 3.0f : 2.2f);

        // Cream cap (knurled edge suggested by fine radial lines)
        const float capR = skirtR * 0.74f;
        const auto cap = juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (c);
        juce::ColourGradient capFill (cream.brighter (0.08f), c.x - capR * 0.4f, c.y - capR * 0.6f,
                                      creamShade, c.x + capR * 0.5f, c.y + capR, true);
        g.setGradientFill (capFill);
        g.fillEllipse (cap);

        g.setColour (creamShade.darker (0.25f).withAlpha (0.55f));
        const int knurls = large ? 72 : 40;
        for (int i = 0; i < knurls; ++i)
        {
            const float ka = juce::MathConstants<float>::twoPi * (float) i / (float) knurls;
            g.drawLine ({ c.getPointOnCircumference (capR - 3.0f, ka), c.getPointOnCircumference (capR, ka) }, 0.8f);
        }
        g.setColour (creamShade.darker (0.4f));
        g.drawEllipse (cap, 1.0f);

        // Pointer on the cap
        g.setColour (s.isEnabled() ? ink : ink.withAlpha (0.35f));
        g.drawLine ({ c.getPointOnCircumference (capR * 0.25f, a), c.getPointOnCircumference (capR - 4.0f, a) }, large ? 3.2f : 2.4f);

        if (! s.isEnabled())
        {
            g.setColour (plateDark.withAlpha (0.45f));
            g.fillEllipse (skirt);
        }
    }

    // ---- Square push button with a lamp -------------------------------------
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool highlighted, bool down) override
    {
        using namespace colours;
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const bool on = b.getToggleState();

        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillRoundedRectangle (r.translated (0.0f, down ? 0.5f : 2.0f), 3.0f);

        auto face = r.translated (0.0f, down ? 1.0f : 0.0f);
        g.setGradientFill (juce::ColourGradient (charcoalHi.brighter (highlighted ? 0.12f : 0.0f), 0.0f, face.getY(),
                                                 charcoal, 0.0f, face.getBottom(), false));
        g.fillRoundedRectangle (face, 3.0f);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (face, 3.0f, 1.0f);

        // Lamp
        auto lamp = juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ face.getX() + 13.0f, face.getCentreY() });
        if (on)
        {
            g.setColour (amber.withAlpha (0.35f));
            g.fillEllipse (lamp.expanded (4.0f));
        }
        g.setGradientFill (juce::ColourGradient (on ? amber.brighter (0.5f) : amberDim.brighter (0.2f), lamp.getCentreX() - 2.0f, lamp.getY(),
                                                 on ? amber.darker (0.2f) : amberDim.darker (0.4f), lamp.getCentreX(), lamp.getBottom(), false));
        g.fillEllipse (lamp);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (lamp, 0.8f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().withTrimmedLeft (22.0f).translated (0.0f, down ? 1.0f : 0.0f);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.setColour (colours::silkCream.withAlpha (b.getToggleState() ? 1.0f : 0.7f));
        g.drawText (b.getButtonText(), r, juce::Justification::centred);
    }

    // ---- Bat-handle toggle switch -------------------------------------------
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool) override
    {
        using namespace colours;
        auto r = b.getLocalBounds().toFloat();
        const bool on = b.getToggleState();
        const float alpha = b.isEnabled() ? 1.0f : 0.45f;

        auto sw = r.removeFromLeft (30.0f).withSizeKeepingCentre (22.0f, juce::jmin (44.0f, r.getHeight()));
        const auto c = sw.getCentre();

        // Hex nut
        juce::Path nut;
        nut.addPolygon (c, 6, 10.0f, 0.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe6e7e9).withMultipliedAlpha (alpha), c.x - 8.0f, c.y - 8.0f,
                                                 juce::Colour (0xff7a7d82).withMultipliedAlpha (alpha), c.x + 8.0f, c.y + 8.0f, false));
        g.fillPath (nut);
        g.setColour (juce::Colour (0xff55585c).withMultipliedAlpha (alpha));
        g.strokePath (nut, juce::PathStrokeType (0.8f));
        g.setColour (juce::Colour (0xff2d2e30).withMultipliedAlpha (alpha));
        g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (c));

        // Lever
        const float dir = on ? -1.0f : 1.0f;
        const auto tip = c.translated (0.0f, dir * (sw.getHeight() * 0.5f - 4.0f));
        g.setColour (juce::Colours::black.withAlpha (0.3f * alpha));
        g.drawLine ({ c.translated (1.5f, 2.0f), tip.translated (1.5f, 2.0f) }, 5.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff4f4f4).withMultipliedAlpha (alpha), c.x - 3.0f, c.y,
                                                 juce::Colour (0xff8d9095).withMultipliedAlpha (alpha), c.x + 3.0f, c.y, false));
        g.drawLine ({ c, tip }, 5.0f);
        g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (tip));
        if (highlighted && b.isEnabled())
        {
            g.setColour (juce::Colours::white.withAlpha (0.3f));
            g.drawEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (tip), 1.0f);
        }

        // Legends
        auto text = r.withTrimmedLeft (4.0f);
        g.saveState();
        g.setOpacity (alpha);
        drawEngravedText (g, "ON",  text.removeFromTop (text.getHeight() * 0.5f), juce::Justification::centredLeft, 10.0f);
        drawEngravedText (g, "OFF", text, juce::Justification::centredLeft, 10.0f);
        g.restoreState();
    }
};

} // namespace audemo
