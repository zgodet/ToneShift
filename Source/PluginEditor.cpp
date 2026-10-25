/*
    ToneShift by Audiemo
*/
#include "PluginEditor.h"
#include "Parameters.h"

using namespace audiemo;

namespace
{
    juce::String signedString (float v, int decimals)
    {
        if (std::abs (v) < 0.5f * std::pow (10.0f, (float) -decimals))
            v = 0.0f;
        return (v > 0.0f ? "+" : "") + juce::String (v, decimals);
    }
}

//==============================================================================
PitchRuler::PitchRuler (juce::RangedAudioParameter& p, juce::RangedAudioParameter& f)
    : pitchParam (p), fineParam (f),
      pitchAttachment (p, [this] (float) { repaint(); }),
      fineAttachment (f, [this] (float) { repaint(); })
{
    pitchAttachment.sendInitialUpdate();
    fineAttachment.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip ("Click or drag to set the pitch in semitones");
}

float PitchRuler::xForSemitone (float st) const
{
    const auto r = getLocalBounds().toFloat().reduced (14.0f, 0.0f);
    return juce::jmap (st, -24.0f, 24.0f, r.getX(), r.getRight());
}

void PitchRuler::paint (juce::Graphics& g)
{
    using namespace colours;
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (panelEdge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);

    const float baseY = bounds.getY() + bounds.getHeight() * 0.58f;
    const float st = pitchParam.convertFrom0to1 (pitchParam.getValue());
    const float ct = fineParam.convertFrom0to1 (fineParam.getValue());
    const float pos = st + ct / 100.0f;

    // Highlight from zero to the current pitch.
    const float x0 = xForSemitone (0.0f), x1 = xForSemitone (pos);
    g.setColour (accentDim.withAlpha (0.55f));
    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (x0, x1), baseY - 3.0f, juce::jmax (x0, x1), baseY + 3.0f));

    for (int s = -24; s <= 24; ++s)
    {
        const float x = xForSemitone ((float) s);
        const bool octave = s % 12 == 0;
        const bool fifth = std::abs (s % 12) == 7 || std::abs (s % 12) == 5;
        const float len = octave ? 14.0f : (fifth ? 9.0f : 5.0f);

        g.setColour (octave ? text : textDim.withAlpha (fifth ? 0.9f : 0.55f));
        g.drawLine (x, baseY - len * 0.5f, x, baseY + len * 0.5f, octave ? 1.6f : 1.0f);

        if (octave || std::abs (s) == 5 || std::abs (s) == 7)
        {
            g.setFont (juce::FontOptions (octave ? 11.5f : 10.0f, octave ? juce::Font::bold : juce::Font::plain));
            g.drawText (s > 0 ? "+" + juce::String (s) : juce::String (s),
                        juce::Rectangle<float> (x - 16.0f, baseY + 9.0f, 32.0f, 14.0f), juce::Justification::centred);
        }
    }

    // Marker
    g.setColour (accent);
    juce::Path tri;
    tri.addTriangle (x1 - 6.0f, bounds.getY() + 5.0f, x1 + 6.0f, bounds.getY() + 5.0f, x1, bounds.getY() + 14.0f);
    g.fillPath (tri);
    g.fillRoundedRectangle (x1 - 1.5f, bounds.getY() + 12.0f, 3.0f, baseY - bounds.getY() - 4.0f, 1.5f);
}

void PitchRuler::setFromX (float x)
{
    const auto r = getLocalBounds().toFloat().reduced (14.0f, 0.0f);
    const float st = std::round (juce::jmap (juce::jlimit (r.getX(), r.getRight(), x), r.getX(), r.getRight(), -24.0f, 24.0f));
    pitchAttachment.setValueAsPartOfGesture (st);
}

void PitchRuler::mouseDown (const juce::MouseEvent& e)
{
    pitchAttachment.beginGesture();
    setFromX ((float) e.x);
}

void PitchRuler::mouseDrag (const juce::MouseEvent& e) { setFromX ((float) e.x); }
void PitchRuler::mouseUp (const juce::MouseEvent&)     { pitchAttachment.endGesture(); }

//==============================================================================
void LevelMeter::setLevel (float newLevel)
{
    newLevel = juce::jlimit (0.0f, 2.0f, newLevel);
    if (std::abs (newLevel - level) > 0.001f)
    {
        level = newLevel;
        repaint();
    }
}

void LevelMeter::paint (juce::Graphics& g)
{
    using namespace colours;
    auto r = getLocalBounds().toFloat();
    auto labelArea = r.removeFromBottom (16.0f);
    auto bar = r.withSizeKeepingCentre (8.0f, r.getHeight());

    g.setColour (track);
    g.fillRoundedRectangle (bar, 4.0f);

    const float db = juce::Decibels::gainToDecibels (level, -60.0f);
    const float prop = juce::jmap (db, -60.0f, 6.0f, 0.0f, 1.0f);
    auto filled = bar.withTrimmedTop (bar.getHeight() * (1.0f - juce::jlimit (0.0f, 1.0f, prop)));
    g.setColour (db > -0.1f ? warn : accent);
    g.fillRoundedRectangle (filled, 4.0f);

    g.setColour (textDim);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText (label, labelArea, juce::Justification::centred);
}

//==============================================================================
ToneShiftEditor::ToneShiftEditor (ToneShiftProcessor& p)
    : AudioProcessorEditor (p), processor (p),
      ruler (*p.apvts.getParameter (params::pitch), *p.apvts.getParameter (params::fine))
{
    setLookAndFeel (&lnf);

    setupKnob (pitch,   pitchLabel,   "PITCH",   true,  0.0);
    setupKnob (fine,    fineLabel,    "FINE",    true,  0.0);
    setupKnob (formant, formantLabel, "SHIFT",   true,  0.0);
    setupKnob (mix,     mixLabel,     "MIX",     false, 100.0);
    setupKnob (output,  outputLabel,  "OUTPUT",  true,  0.0);

    pitch.getProperties().set ("large", true);
    pitch.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    pitch.setMouseDragSensitivity (400);
    pitch.setTooltip ("Pitch shift in semitones (double-click to reset)");
    fine.setTooltip ("Fine tuning in cents");
    formant.setTooltip ("Moves the vocal character (formants) independently of pitch");
    correction.setTooltip ("Keep the original formants so voices don't sound like chipmunks or giants");

    auto& apvts = p.apvts;
    pitchAtt   = std::make_unique<SliderAttachment> (apvts, params::pitch, pitch);
    fineAtt    = std::make_unique<SliderAttachment> (apvts, params::fine, fine);
    formantAtt = std::make_unique<SliderAttachment> (apvts, params::formant, formant);
    mixAtt     = std::make_unique<SliderAttachment> (apvts, params::mix, mix);
    outputAtt  = std::make_unique<SliderAttachment> (apvts, params::output, output);

    addAndMakeVisible (correction);
    correctionAtt = std::make_unique<ButtonAttachment> (apvts, params::preserve, correction);
    correction.onStateChange = [this] { updateFormantEnablement(); };

    // Mode: two segmented buttons bound to the choice parameter.
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
            repaint();
        });
    qualityButton.onClick    = [this] { modeAtt->setValueAsCompleteGesture (0.0f); };
    lowLatencyButton.onClick = [this] { modeAtt->setValueAsCompleteGesture (1.0f); };
    modeAtt->sendInitialUpdate();

    addAndMakeVisible (ruler);
    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);

    pitch.onValueChange = [this] { repaint (pitchArea); repaint (fineArea); };
    fine.onValueChange  = [this] { repaint (pitchArea); repaint (fineArea); };

    updateFormantEnablement();
    sendLookAndFeelChange(); // rebuild slider text boxes with our look
    setSize (820, 470);
    startTimerHz (30);
}

ToneShiftEditor::~ToneShiftEditor()
{
    setLookAndFeel (nullptr);
}

void ToneShiftEditor::setupKnob (juce::Slider& s, juce::Label& l, const juce::String& name, bool bipolar, double resetValue)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 18);
    s.setDoubleClickReturnValue (true, resetValue);
    s.getProperties().set ("bipolar", bipolar);
    addAndMakeVisible (s);

    l.setText (name, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    l.setColour (juce::Label::textColourId, colours::textDim);
    addAndMakeVisible (l);
}

void ToneShiftEditor::updateFormantEnablement()
{
    const bool spectralMode = ! lowLatencyButton.getToggleState();
    correction.setEnabled (spectralMode);
    formant.setEnabled (spectralMode && correction.getToggleState());
    formantLabel.setEnabled (formant.isEnabled());
    formantLabel.setAlpha (formant.isEnabled() ? 1.0f : 0.4f);
    correction.setAlpha (spectralMode ? 1.0f : 0.4f);
}

void ToneShiftEditor::timerCallback()
{
    inMeter.setLevel (processor.inputLevel.load());
    outMeter.setLevel (processor.outputLevel.load());
}

void ToneShiftEditor::drawSection (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title)
{
    using namespace colours;
    g.setColour (panel);
    g.fillRoundedRectangle (r.toFloat(), 10.0f);
    g.setColour (panelEdge);
    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 10.0f, 1.0f);

    g.setColour (textDim);
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    g.drawText (title, r.withHeight (26).withTrimmedLeft (14), juce::Justification::centredLeft);
}

void ToneShiftEditor::paint (juce::Graphics& g)
{
    using namespace colours;
    g.fillAll (background);

    // Header
    auto header = getLocalBounds().removeFromTop (60).reduced (20, 0);
    g.setColour (accent);
    g.fillRoundedRectangle (juce::Rectangle<float> ((float) header.getX(), 22.0f, 4.0f, 18.0f), 2.0f);

    g.setColour (text);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("ToneShift", header.withTrimmedLeft (14), juce::Justification::centredLeft);

    g.setColour (textDim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("A U D I E M O", header.withTrimmedLeft (130), juce::Justification::centredLeft);

    drawSection (g, fineArea, "TUNING");
    drawSection (g, pitchArea, "PITCH");
    drawSection (g, formantArea, "FORMANT");
    drawSection (g, outputArea, "OUTPUT");

    // Tuning summary under the fine knob
    const float semis = (float) pitch.getValue() + (float) fine.getValue() * 0.01f;
    const float ratio = std::pow (2.0f, semis / 12.0f);
    auto info = fineArea.reduced (14).removeFromBottom (64);
    g.setFont (juce::FontOptions (11.0f));
    auto row = [&] (const juce::String& k, const juce::String& v)
    {
        auto line = info.removeFromTop (20);
        g.setColour (textDim);
        g.drawText (k, line, juce::Justification::centredLeft);
        g.setColour (text);
        g.drawText (v, line, juce::Justification::centredRight);
    };
    row ("Total", signedString (semis * 100.0f, 0) + " ct");
    row ("Ratio", juce::String::charToString (0x00d7) + juce::String (ratio, 3));
    const bool low = lowLatencyButton.getToggleState();
    const double sr = processor.getSampleRate() > 0 ? processor.getSampleRate() : 48000.0;
    const int latency = processor.getLatencySamples();
    row ("Latency", low ? juce::String ("none")
                        : (latency > 0 ? juce::String (latency * 1000.0 / sr, 1) + " ms" : juce::String ("-")));

    // Footer
    auto footer = getLocalBounds().removeFromBottom (34).reduced (22, 0);
    g.setColour (textDim.withAlpha (0.7f));
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("Audiemo ToneShift  v" JucePlugin_VersionString, footer, juce::Justification::centredLeft);
    g.drawText ("Double-click a control to reset", footer, juce::Justification::centredRight);
}

void ToneShiftEditor::paintOverChildren (juce::Graphics& g)
{
    using namespace colours;
    // Big readout inside the pitch knob.
    const auto c = pitch.getBounds().toFloat().getCentre();
    const float st = (float) pitch.getValue();
    const float ct = (float) fine.getValue();

    g.setColour (text);
    g.setFont (juce::FontOptions (40.0f, juce::Font::bold));
    g.drawText (signedString (st, 0), juce::Rectangle<float> (140.0f, 46.0f).withCentre (c.translated (0.0f, -8.0f)),
                juce::Justification::centred);

    g.setColour (textDim);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    const juce::String sub = std::abs (ct) > 0.5f ? "SEMITONES  " + signedString (ct, 0) + " ct" : juce::String ("SEMITONES");
    g.drawText (sub, juce::Rectangle<float> (140.0f, 16.0f).withCentre (c.translated (0.0f, 22.0f)),
                juce::Justification::centred);
}

void ToneShiftEditor::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (60).reduced (20, 14);
    lowLatencyButton.setBounds (header.removeFromRight (110));
    header.removeFromRight (6);
    qualityButton.setBounds (header.removeFromRight (90));

    r.removeFromBottom (34);
    r.reduce (20, 0);

    auto rulerArea = r.removeFromBottom (64);
    ruler.setBounds (rulerArea);
    r.removeFromBottom (14);

    auto main = r.withTrimmedTop (4);
    fineArea = main.removeFromLeft (164);
    main.removeFromLeft (14);
    outputArea = main.removeFromRight (164);
    main.removeFromRight (14);
    formantArea = main.removeFromRight (164);
    main.removeFromRight (14);
    pitchArea = main;

    auto placeKnob = [] (juce::Slider& s, juce::Label& l, juce::Rectangle<int> area)
    {
        l.setBounds (area.removeFromTop (16));
        s.setBounds (area);
    };

    // Tuning
    {
        auto a = fineArea.reduced (12).withTrimmedTop (18);
        placeKnob (fine, fineLabel, a.removeFromTop (120));
    }

    // Pitch (large)
    {
        auto a = pitchArea.reduced (12).withTrimmedTop (14);
        pitchLabel.setVisible (false);
        pitch.setBounds (a.withSizeKeepingCentre (juce::jmin (a.getWidth(), a.getHeight()), juce::jmin (a.getWidth(), a.getHeight())));
    }

    // Formant
    {
        auto a = formantArea.reduced (12).withTrimmedTop (20);
        correction.setBounds (a.removeFromTop (24).reduced (4, 0));
        a.removeFromTop (8);
        placeKnob (formant, formantLabel, a.removeFromTop (120));
    }

    // Output: mix + gain knobs, meters on the right
    {
        auto a = outputArea.reduced (10).withTrimmedTop (18);
        auto meters = a.removeFromRight (44);
        inMeter.setBounds (meters.removeFromLeft (22));
        outMeter.setBounds (meters);
        const int h = a.getHeight() / 2;
        placeKnob (mix, mixLabel, a.removeFromTop (h - 4));
        a.removeFromTop (8);
        placeKnob (output, outputLabel, a);
    }
}
