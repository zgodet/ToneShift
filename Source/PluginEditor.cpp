/*
    ToneShift by Audemo
*/
#include "PluginEditor.h"
#include "Parameters.h"

using namespace audemo;

namespace
{
    juce::String signedString (float v, int decimals)
    {
        if (std::abs (v) < 0.5f * std::pow (10.0f, (float) -decimals))
            v = 0.0f;
        return (v > 0.0f ? "+" : "") + juce::String (v, decimals);
    }

    juce::Font monoFont (float size)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, juce::Font::bold));
    }

    // Amber text with a soft glow around it.
    void drawGlowText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r,
                       juce::Justification just, const juce::Font& font, float alpha = 1.0f)
    {
        g.setFont (font);
        g.setColour (colours::amber.withAlpha (0.13f * alpha));
        for (auto d : { juce::Point<float> (-1.5f, 0.0f), { 1.5f, 0.0f }, { 0.0f, -1.5f }, { 0.0f, 1.5f } })
            g.drawText (text, r.translated (d.x, d.y), just, false);
        g.setColour (colours::amber.withAlpha (alpha));
        g.drawText (text, r, just, false);
    }

    constexpr float knobStart = juce::MathConstants<float>::pi * 1.25f;
    constexpr float knobEnd   = juce::MathConstants<float>::pi * 2.75f;
}

//==============================================================================
void AmberDisplay::setReadout (float s, float c, float r, const juce::String& m, const juce::String& l)
{
    using juce::exactlyEqual;
    if (! exactlyEqual (s, semis) || ! exactlyEqual (c, ct) || ! exactlyEqual (r, ratio) || m != mode || l != latency)
    {
        semis = s; ct = c; ratio = r; mode = m; latency = l;
        repaint();
    }
}

void AmberDisplay::showMessage (const juce::String& name, const juce::String& value)
{
    msgName = name;
    msgValue = value;
    msgFrames = 45; // ~1.5 s at 30 Hz
    repaint();
}

void AmberDisplay::tick()
{
    if (msgFrames > 0 && --msgFrames == 0)
        repaint();
}

void AmberDisplay::paint (juce::Graphics& g)
{
    using namespace colours;
    auto r = getLocalBounds().toFloat();

    // Bezel and glass
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1a1b1d), 0.0f, r.getY(), juce::Colour (0xff6b6d71), 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, 6.0f);
    auto glassArea = r.reduced (4.0f);
    g.setColour (glass);
    g.fillRoundedRectangle (glassArea, 4.0f);
    g.setGradientFill (juce::ColourGradient (amber.withAlpha (0.08f), glassArea.getCentreX(), glassArea.getCentreY(),
                                             juce::Colours::transparentBlack, glassArea.getX(), glassArea.getY(), true));
    g.fillRoundedRectangle (glassArea, 4.0f);

    auto content = glassArea.reduced (14.0f, 8.0f);
    auto big = content.removeFromLeft (content.getWidth() * 0.46f);

    // Large semitone readout over unlit "segments"
    const auto bigFont = monoFont (big.getHeight() * 0.78f);
    auto digits = big.withTrimmedRight (34.0f);
    g.setFont (bigFont);
    g.setColour (amberDim.withAlpha (0.35f));
    g.drawText ("+88", digits, juce::Justification::centredRight, false);

    const int whole = (int) std::lround (semis);
    juce::String num = (whole > 0 ? "+" : (whole < 0 ? "-" : " ")) + juce::String (std::abs (whole)).paddedLeft ('0', 2);
    drawGlowText (g, num, digits, juce::Justification::centredRight, bigFont);
    drawGlowText (g, "ST", big.removeFromRight (30.0f).withTrimmedBottom (big.getHeight() * 0.18f),
                  juce::Justification::bottomLeft, monoFont (13.0f), 0.85f);

    // Right column: details or the control being moved
    content.removeFromLeft (10.0f);
    g.setColour (amberDim.withAlpha (0.8f));
    g.fillRect (content.removeFromLeft (1.0f));
    content.removeFromLeft (12.0f);

    if (msgFrames > 0)
    {
        drawGlowText (g, msgName, content.removeFromTop (content.getHeight() * 0.42f), juce::Justification::bottomLeft, monoFont (13.0f), 0.75f);
        drawGlowText (g, msgValue, content, juce::Justification::topLeft, monoFont (24.0f));
        return;
    }

    const float lineH = content.getHeight() / 3.0f;
    auto line = [&] (const juce::String& k, const juce::String& v)
    {
        auto row = content.removeFromTop (lineH);
        drawGlowText (g, k, row, juce::Justification::centredLeft, monoFont (12.0f), 0.6f);
        drawGlowText (g, v, row, juce::Justification::centredRight, monoFont (14.0f));
    };
    line ("FINE",  signedString (ct, 0) + " CT");
    line ("RATIO", juce::String (ratio, 3));
    line (mode, latency);
}

//==============================================================================
void VuMeter::setPeak (float linearPeak)
{
    // 0 VU at -18 dBFS; needle eases toward the target like a real movement.
    const float target = juce::jlimit (-20.0f, 3.5f, juce::Decibels::gainToDecibels (linearPeak, -60.0f) + 18.0f);
    const float coeff = target > needle ? 0.35f : 0.12f;
    const float next = needle + (target - needle) * coeff;

    const bool clip = linearPeak >= 0.99f;
    const int hold = clip ? 30 : juce::jmax (0, peakHold - 1);

    if (std::abs (next - needle) > 0.01f || (hold > 0) != (peakHold > 0))
    {
        needle = next;
        peakHold = hold;
        repaint();
    }
    else
    {
        needle = next;
        peakHold = hold;
    }
}

void VuMeter::paint (juce::Graphics& g)
{
    using namespace colours;
    auto r = getLocalBounds().toFloat();

    // Bezel and cream face
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 6.0f);
    g.setGradientFill (juce::ColourGradient (charcoalHi, 0.0f, r.getY(), charcoal, 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, 6.0f);
    auto face = r.reduced (6.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe8cb84), face.getCentreX(), face.getBottom(),
                                             juce::Colour (0xffa9864a), face.getX(), face.getY(), true));
    g.fillRoundedRectangle (face, 3.0f);

    const float span = 0.85f; // radians either side of vertical
    // Fit the scale (plus its labels) inside the face both ways.
    const float radius = juce::jmin (face.getHeight() * 0.92f, (face.getWidth() * 0.5f - 24.0f) / std::sin (span));
    const juce::Point<float> pivot { face.getCentreX(), face.getY() + 30.0f + radius };

    auto angleFor = [&] (float vu)
    {
        // Real VU scales are non-linear; this mapping crowds the low end the same way.
        const float lin = std::pow (10.0f, vu / 20.0f);
        const float lo = std::pow (10.0f, -20.0f / 20.0f), hi = std::pow (10.0f, 3.0f / 20.0f);
        return -span + 2.0f * span * (lin - lo) / (hi - lo);
    };

    g.saveState();
    g.reduceClipRegion (face.toNearestInt());

    // Scale arc (black, red above 0)
    juce::Path arc, red;
    arc.addCentredArc (pivot.x, pivot.y, radius, radius, 0.0f, angleFor (-20.0f), angleFor (0.0f), true);
    red.addCentredArc (pivot.x, pivot.y, radius, radius, 0.0f, angleFor (0.0f), angleFor (3.0f), true);
    g.setColour (ink);
    g.strokePath (arc, juce::PathStrokeType (1.4f));
    g.setColour (lampRed);
    g.strokePath (red, juce::PathStrokeType (4.0f));

    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    for (int mark : { -20, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3 })
    {
        const float vu = (float) mark;
        const float a = angleFor (vu);
        auto p1 = pivot.getPointOnCircumference (radius, a);
        auto p2 = pivot.getPointOnCircumference (radius + 6.0f, a);
        g.setColour (mark > 0 ? lampRed : ink);
        g.drawLine ({ p1, p2 }, 1.2f);
        if (mark == -20 || mark == -10 || mark == -5 || mark == -3 || mark == 0 || mark == 3)
        {
            auto lp = pivot.getPointOnCircumference (radius + 15.0f, a);
            const juce::String txt = mark > 0 ? "+" + juce::String (mark) : juce::String (std::abs (mark));
            g.drawText (txt, juce::Rectangle<float> (24.0f, 12.0f).withCentre (lp), juce::Justification::centred);
        }
    }

    g.setColour (ink);
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.drawText ("VU", face.withTrimmedTop (face.getHeight() * 0.45f).withHeight (20.0f), juce::Justification::centred);

    // Needle
    const float na = angleFor (needle);
    const auto tip = pivot.getPointOnCircumference (radius + 4.0f, na);
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.drawLine ({ pivot.translated (2.0f, 0.0f), tip.translated (2.0f, 1.0f) }, 1.6f);
    g.setColour (juce::Colour (0xff1a1a1a));
    g.drawLine ({ pivot, tip }, 1.4f);
    g.restoreState();

    // Peak lamp
    auto lamp = juce::Rectangle<float> (8.0f, 8.0f).withPosition (face.getRight() - 14.0f, face.getY() + 6.0f);
    if (peakHold > 0)
    {
        g.setColour (lampRed.withAlpha (0.35f));
        g.fillEllipse (lamp.expanded (3.0f));
    }
    g.setColour (peakHold > 0 ? lampRed.brighter (0.3f) : lampRed.darker (1.6f));
    g.fillEllipse (lamp);
    g.setColour (ink);
    g.setFont (juce::FontOptions (8.0f, juce::Font::bold));
    g.drawText ("PEAK", lamp.translated (-30.0f, 0.0f).withWidth (26.0f), juce::Justification::centredRight);
}

//==============================================================================
ToneShiftEditor::ToneShiftEditor (ToneShiftProcessor& p)
    : AudioProcessorEditor (p), processor (p)
{
    setLookAndFeel (&lnf);

    setupKnob (pitch,   "PITCH",   0.0,   49);
    setupKnob (fine,    "FINE",    0.0,   21);
    setupKnob (formant, "SHIFT",   0.0,   25);
    setupKnob (mix,     "MIX",     100.0, 11);
    setupKnob (output,  "OUTPUT",  0.0,   13);

    pitch.getProperties().set ("large", true);
    pitch.getProperties().set ("majorEvery", 12);
    fine.getProperties().set ("majorEvery", 10);
    formant.getProperties().set ("majorEvery", 12);
    output.getProperties().set ("majorEvery", 8); // -24, -8 ... 0 dB sits on a minor tick otherwise
    pitch.setMouseDragSensitivity (400);

    pitch.setTooltip ("Pitch shift in semitones (double-click to reset)");
    fine.setTooltip ("Fine tuning in cents");
    formant.setTooltip ("Moves the vocal character (formants) independently of pitch");
    correction.setTooltip ("Formant correction: keeps voices from sounding like chipmunks or giants");
    mix.setTooltip ("Dry/wet blend (latency aligned)");
    output.setTooltip ("Output level");

    auto& apvts = p.apvts;
    pitchAtt   = std::make_unique<SliderAttachment> (apvts, params::pitch, pitch);
    fineAtt    = std::make_unique<SliderAttachment> (apvts, params::fine, fine);
    formantAtt = std::make_unique<SliderAttachment> (apvts, params::formant, formant);
    mixAtt     = std::make_unique<SliderAttachment> (apvts, params::mix, mix);
    outputAtt  = std::make_unique<SliderAttachment> (apvts, params::output, output);

    addAndMakeVisible (correction);
    correctionAtt = std::make_unique<ButtonAttachment> (apvts, params::preserve, correction);
    correction.onStateChange = [this] { updateFormantEnablement(); };

    for (auto* b : { &qualityButton, &lowLatencyButton })
    {
        b->setClickingTogglesState (false);
        addAndMakeVisible (*b);
    }
    qualityButton.setTooltip ("Spectral engine: cleanest sound, formant control, ~43 ms latency (compensated by the host)");
    lowLatencyButton.setTooltip ("Time-domain engine: no added latency, ideal for live monitoring");

    modeAtt = std::make_unique<juce::ParameterAttachment> (*apvts.getParameter (params::mode),
        [this] (float v)
        {
            const bool low = v > 0.5f;
            qualityButton.setToggleState (! low, juce::dontSendNotification);
            lowLatencyButton.setToggleState (low, juce::dontSendNotification);
            updateFormantEnablement();
            updateDisplay();
        });
    qualityButton.onClick    = [this] { modeAtt->setValueAsCompleteGesture (0.0f); };
    lowLatencyButton.onClick = [this] { modeAtt->setValueAsCompleteGesture (1.0f); };
    modeAtt->sendInitialUpdate();

    addAndMakeVisible (display);
    addAndMakeVisible (vu);

    pitch.onValueChange = [this] { updateDisplay(); };
    fine.onValueChange  = [this] { updateDisplay(); };
    for (auto* s : { &formant, &mix, &output })
        s->onValueChange = [this, s] { display.showMessage (s->getName(), s->getTextFromValue (s->getValue()).toUpperCase()); };

    updateFormantEnablement();
    updateDisplay();
    setSize (820, 470);
    startTimerHz (30);
}

ToneShiftEditor::~ToneShiftEditor()
{
    setLookAndFeel (nullptr);
}

void ToneShiftEditor::setupKnob (juce::Slider& s, const juce::String& name, double resetValue, int ticks)
{
    s.setName (name);
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setRotaryParameters (knobStart, knobEnd, true);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setDoubleClickReturnValue (true, resetValue);
    s.getProperties().set ("ticks", ticks);
    addAndMakeVisible (s);
}

void ToneShiftEditor::updateFormantEnablement()
{
    const bool spectralMode = ! lowLatencyButton.getToggleState();
    correction.setEnabled (spectralMode);
    formant.setEnabled (spectralMode && correction.getToggleState());
    repaint (formantGroup);
}

void ToneShiftEditor::updateDisplay()
{
    const float st = (float) pitch.getValue();
    const float ct = (float) fine.getValue();
    const float ratio = std::pow (2.0f, (st + ct * 0.01f) / 12.0f);
    const bool low = lowLatencyButton.getToggleState();

    juce::String lat = "0 MS";
    if (! low)
    {
        const int samples = processor.getLatencySamples();
        const double sr = processor.getSampleRate();
        lat = (samples > 0 && sr > 0) ? juce::String (samples * 1000.0 / sr, 1) + " MS" : juce::String ("--");
    }

    display.setReadout (st, ct, ratio, low ? "LOW LAT" : "QUALITY", lat);
}

void ToneShiftEditor::timerCallback()
{
    vu.setPeak (processor.outputLevel.load());
    display.tick();
    updateDisplay(); // picks up latency once the host has prepared the plugin
}

void ToneShiftEditor::renderFaceplate()
{
    using namespace colours;
    const int w = juce::jmax (1, getWidth()), h = juce::jmax (1, getHeight());
    faceplate = juce::Image (juce::Image::RGB, w, h, true);
    juce::Graphics g (faceplate);

    g.setGradientFill (juce::ColourGradient (plateLight, 0.0f, 0.0f, plateDark, 0.0f, (float) h, false));
    g.fillAll();

    // Brushed grain: fixed seed so the plate looks the same every time.
    juce::Random rnd (0x7a11);
    for (int y = 0; y < h; ++y)
    {
        g.setColour ((rnd.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (rnd.nextFloat() * 0.05f));
        g.drawHorizontalLine (y, 0.0f, (float) w);
    }
    for (int i = 0; i < 900; ++i)
    {
        const float y = rnd.nextFloat() * (float) h;
        const float x = rnd.nextFloat() * (float) w;
        const float len = 40.0f + rnd.nextFloat() * 220.0f;
        g.setColour ((rnd.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.02f + rnd.nextFloat() * 0.035f));
        g.drawHorizontalLine ((int) y, x, x + len);
    }

    // Soft light falloff
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.07f), (float) w * 0.3f, 0.0f,
                                             juce::Colours::transparentBlack, (float) w * 0.3f, (float) h * 0.7f, true));
    g.fillAll();
}

void ToneShiftEditor::drawKnobLegend (juce::Graphics& g, const juce::Slider& s, const juce::String& name,
                                      const juce::String& left, const juce::String& right)
{
    const auto b = s.getBounds().toFloat();
    const auto c = b.getCentre();
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const float alpha = s.isEnabled() ? 1.0f : 0.45f;

    g.saveState();
    g.setOpacity (alpha);
    drawEngravedText (g, name, juce::Rectangle<float> (b.getWidth() + 40.0f, 16.0f).withCentre ({ c.x, b.getY() - 9.0f }),
                      juce::Justification::centred, 12.0f);

    const auto pl = c.getPointOnCircumference (r + 2.0f, knobStart).translated (4.0f, 10.0f);
    const auto pr = c.getPointOnCircumference (r + 2.0f, knobEnd).translated (-4.0f, 10.0f);
    drawEngravedText (g, left,  juce::Rectangle<float> (44.0f, 12.0f).withCentre (pl), juce::Justification::centred, 9.5f, false);
    drawEngravedText (g, right, juce::Rectangle<float> (44.0f, 12.0f).withCentre (pr), juce::Justification::centred, 9.5f, false);
    g.restoreState();
}

void ToneShiftEditor::paint (juce::Graphics& g)
{
    using namespace colours;
    if (faceplate.isNull() || faceplate.getWidth() != getWidth() || faceplate.getHeight() != getHeight())
        renderFaceplate();
    g.drawImageAt (faceplate, 0, 0);

    // Header strip
    auto hb = header.toFloat();
    g.setGradientFill (juce::ColourGradient (charcoalHi, 0.0f, hb.getY(), charcoal, 0.0f, hb.getBottom(), false));
    g.fillRect (hb);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRect (hb.withTop (hb.getBottom()).withHeight (2.0f));

    auto text = hb.reduced (26.0f, 0.0f);
    g.setColour (silkCream);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("A U D E M O", text, juce::Justification::centredLeft);
    g.setColour (amber);
    g.fillEllipse (text.getX() + 100.0f, hb.getCentreY() - 3.0f, 6.0f, 6.0f);
    g.setColour (silkCream);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("ToneShift", text.withTrimmedLeft (130.0f), juce::Justification::centredLeft);
    g.setColour (silkCream.withAlpha (0.55f));
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    g.drawText ("PITCH PROCESSOR", text.withTrimmedLeft (248.0f), juce::Justification::centredLeft);

    // Screws
    const float sy1 = hb.getBottom() + 14.0f, sy2 = (float) getHeight() - 14.0f;
    drawScrew (g, { 14.0f, sy1 }, 5.5f, 0.6f);
    drawScrew (g, { (float) getWidth() - 14.0f, sy1 }, 5.5f, 2.1f);
    drawScrew (g, { 14.0f, sy2 }, 5.5f, 1.2f);
    drawScrew (g, { (float) getWidth() - 14.0f, sy2 }, 5.5f, 0.2f);

    // Knob legends
    drawKnobLegend (g, fine,    "FINE",    "-100", "+100");
    drawKnobLegend (g, formant, "SHIFT",   "-12",  "+12");
    drawKnobLegend (g, mix,     "MIX",     "DRY",  "WET");
    drawKnobLegend (g, output,  "OUTPUT",  "-24",  "+12");

    // Pitch dial numbers
    {
        const auto b = pitch.getBounds().toFloat();
        const auto c = b.getCentre();
        const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f + 20.0f;
        for (int st : { -24, -12, 0, 12, 24 })
        {
            const float a = knobStart + (knobEnd - knobStart) * (float) (st + 24) / 48.0f;
            const auto p = c.getPointOnCircumference (r, a);
            drawEngravedText (g, st > 0 ? "+" + juce::String (st) : juce::String (st),
                              juce::Rectangle<float> (34.0f, 14.0f).withCentre (p), juce::Justification::centred, 11.5f);
        }
        drawEngravedText (g, "SEMITONES", juce::Rectangle<float> (120.0f, 14.0f).withCentre ({ c.x, b.getBottom() - 6.0f }),
                          juce::Justification::centred, 10.0f);
    }

    // Formant group outline
    {
        auto fg = formantGroup.toFloat();
        g.setColour (engraveHi);
        g.drawRoundedRectangle (fg.translated (0.0f, 1.0f), 6.0f, 1.0f);
        g.setColour (engraving.withAlpha (0.6f));
        g.drawRoundedRectangle (fg, 6.0f, 1.0f);
        auto title = juce::Rectangle<float> (fg.getX() + 12.0f, fg.getY() - 7.0f, 84.0f, 14.0f);
        g.saveState();
        g.reduceClipRegion (title.toNearestInt());
        g.drawImageAt (faceplate, 0, 0);
        g.restoreState();
        drawEngravedText (g, "FORMANT", title, juce::Justification::centred, 11.0f);
        g.saveState();
        g.setOpacity (correction.isEnabled() ? 1.0f : 0.45f);
        drawEngravedText (g, "CORRECTION", correction.getBounds().toFloat().withY ((float) correction.getBottom() + 4.0f).withHeight (14.0f).expanded (20.0f, 0.0f),
                          juce::Justification::centred, 10.0f);
        g.restoreState();
    }

    // Model plate
    drawEngravedText (g, juce::String (juce::CharPointer_UTF8 ("MODEL TS-1   \xc2\xb7   AUDEMO   \xc2\xb7   v")) + JucePlugin_VersionString,
                      juce::Rectangle<float> (0.0f, (float) getHeight() - 24.0f, (float) getWidth(), 14.0f),
                      juce::Justification::centred, 9.5f);
}

void ToneShiftEditor::resized()
{
    auto r = getLocalBounds();
    header = r.removeFromTop (58);
    {
        auto h = header.reduced (24, 14);
        lowLatencyButton.setBounds (h.removeFromRight (128));
        h.removeFromRight (10);
        qualityButton.setBounds (h.removeFromRight (104));
    }

    r.removeFromBottom (28);
    r.reduce (34, 0);
    r.removeFromTop (30);

    auto left = r.removeFromLeft (190);
    auto right = r.removeFromRight (210);
    auto centre = r.reduced (16, 0);

    // Left: fine, then formant group
    fine.setBounds (left.removeFromTop (128).withSizeKeepingCentre (118, 118));
    left.removeFromTop (26);
    formantGroup = left.withTrimmedBottom (8);
    {
        auto fg = formantGroup.reduced (12, 14).withTrimmedTop (6);
        auto sw = fg.removeFromLeft (64);
        correction.setBounds (sw.withSizeKeepingCentre (52, 46).translated (0, -12));
        formant.setBounds (fg.withSizeKeepingCentre (juce::jmin (fg.getWidth(), fg.getHeight() - 14), juce::jmin (fg.getWidth(), fg.getHeight() - 14))
                             .translated (0, 8));
    }

    // Centre: amber display above the big pitch dial
    display.setBounds (centre.removeFromTop (92));
    centre.removeFromTop (24);
    const int dial = juce::jmin (centre.getWidth() - 60, centre.getHeight() - 18);
    pitch.setBounds (centre.withSizeKeepingCentre (dial, dial).withY (centre.getY() + 4));

    // Right: VU, then mix and output
    vu.setBounds (right.removeFromTop (140));
    right.removeFromTop (52);
    auto knobs = right.removeFromTop (110);
    mix.setBounds (knobs.removeFromLeft (knobs.getWidth() / 2).withSizeKeepingCentre (92, 92));
    output.setBounds (knobs.withSizeKeepingCentre (92, 92));
}
