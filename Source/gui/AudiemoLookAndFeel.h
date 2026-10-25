/*
    ToneShift by Audiemo
    Shared colours and control drawing.
*/
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace audiemo
{

namespace colours
{
    inline const juce::Colour background { 0xff121419 };
    inline const juce::Colour panel      { 0xff1b1e25 };
    inline const juce::Colour panelEdge  { 0xff2a2e38 };
    inline const juce::Colour track      { 0xff2c313c };
    inline const juce::Colour accent     { 0xff3fd0c0 };
    inline const juce::Colour accentDim  { 0xff1f6e66 };
    inline const juce::Colour text       { 0xffe7e9ee };
    inline const juce::Colour textDim    { 0xff8a919e };
    inline const juce::Colour warn       { 0xfff2b544 };
}

class AudiemoLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    AudiemoLookAndFeel()
    {
        using namespace colours;
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId, accentDim);
        setColour (juce::TextEditor::focusedOutlineColourId, accent);
        setColour (juce::Label::textColourId, text);
        setColour (juce::TextButton::buttonColourId, panel);
        setColour (juce::TextButton::buttonOnColourId, accent);
        setColour (juce::TextButton::textColourOffId, textDim);
        setColour (juce::TextButton::textColourOnId, background);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        using namespace colours;
        const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float thickness = juce::jmax (3.0f, radius * 0.085f);
        const float arcRadius = radius - thickness;
        const bool bipolar = s.getProperties().getWithDefault ("bipolar", false);
        const bool enabled = s.isEnabled();
        const auto valueColour = enabled ? accent : textDim.withAlpha (0.5f);

        // Track
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
        g.setColour (colours::track);
        g.strokePath (track, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Value arc (from zero for bipolar controls)
        const float valueAngle = startAngle + pos * (endAngle - startAngle);
        const float originAngle = bipolar ? startAngle + (float) s.valueToProportionOfLength (0.0) * (endAngle - startAngle)
                                          : startAngle;
        if (std::abs (valueAngle - originAngle) > 0.001f)
        {
            juce::Path value;
            value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                                 juce::jmin (originAngle, valueAngle), juce::jmax (originAngle, valueAngle), true);
            g.setColour (valueColour);
            g.strokePath (value, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Knob body
        const float bodyRadius = arcRadius - thickness * 1.6f;
        g.setGradientFill (juce::ColourGradient (panelEdge.brighter (0.15f), centre.x, centre.y - bodyRadius,
                                                 panel, centre.x, centre.y + bodyRadius, false));
        g.fillEllipse (juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre));
        g.setColour (panelEdge.brighter (0.2f));
        g.drawEllipse (juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre), 1.0f);

        // Pointer
        const float inner = bodyRadius * (s.getProperties().getWithDefault ("large", false) ? 0.78f : 0.35f);
        const auto p1 = centre.getPointOnCircumference (inner, valueAngle);
        const auto p2 = centre.getPointOnCircumference (bodyRadius - 3.0f, valueAngle);
        g.setColour (enabled ? text : textDim);
        g.drawLine ({ p1, p2 }, juce::jmax (2.0f, thickness * 0.55f));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool highlighted, bool down) override
    {
        using namespace colours;
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        auto fill = on ? accent : panel;
        if (! on && (highlighted || down))
            fill = fill.brighter (0.08f);
        g.setColour (fill);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (on ? accent : panelEdge);
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int h) override
    {
        return juce::Font (juce::FontOptions (juce::jmin (13.0f, (float) h * 0.5f), juce::Font::bold));
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool) override
    {
        using namespace colours;
        auto r = b.getLocalBounds().toFloat();
        const bool on = b.getToggleState();

        auto pill = r.removeFromLeft (34.0f).withSizeKeepingCentre (34.0f, 18.0f);
        g.setColour (on ? accent : track);
        g.fillRoundedRectangle (pill, 9.0f);
        const float knobX = on ? pill.getRight() - 16.0f : pill.getX() + 2.0f;
        g.setColour (on ? background : (highlighted ? text : textDim));
        g.fillEllipse (knobX, pill.getY() + 2.0f, 14.0f, 14.0f);

        g.setColour (on ? text : textDim);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (b.getButtonText(), r.withTrimmedLeft (8.0f), juce::Justification::centredLeft);
    }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (juce::FontOptions (13.0f));
        l->setJustificationType (juce::Justification::centred);
        l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::outlineWhenEditingColourId, colours::accent);
        l->setColour (juce::Label::backgroundWhenEditingColourId, colours::background);
        l->setColour (juce::Label::textWhenEditingColourId, colours::text);
        return l;
    }
};

} // namespace audiemo
