/*
    ToneShift by Audemo
*/
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "gui/AudemoLookAndFeel.h"

/** Amber glass display showing pitch, and briefly the control being moved. */
class AmberDisplay final : public juce::Component
{
public:
    void setReadout (float semitones, float cents, float ratio, const juce::String& modeText, const juce::String& latencyText);
    void showMessage (const juce::String& name, const juce::String& value);
    void tick();
    void paint (juce::Graphics&) override;

private:
    float semis = 0.0f, ct = 0.0f, ratio = 1.0f;
    juce::String mode, latency, msgName, msgValue;
    int msgFrames = 0;
};

/** Analogue VU meter with needle ballistics and a peak lamp. */
class VuMeter final : public juce::Component
{
public:
    void setPeak (float linearPeak);
    void paint (juce::Graphics&) override;

private:
    float needle = -20.0f; // VU
    int peakHold = 0;
};

class ToneShiftEditor final : public juce::AudioProcessorEditor,
                              private juce::Timer
{
public:
    explicit ToneShiftEditor (ToneShiftProcessor&);
    ~ToneShiftEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void setupKnob (juce::Slider&, const juce::String& name, double resetValue, int ticks);
    void updateFormantEnablement();
    void updateDisplay();
    void renderFaceplate();
    void drawKnobLegend (juce::Graphics&, const juce::Slider&, const juce::String& name,
                         const juce::String& left, const juce::String& right);

    ToneShiftProcessor& processor;
    audemo::AudemoLookAndFeel lnf;

    juce::Slider pitch, fine, formant, mix, output;
    juce::ToggleButton correction;
    juce::TextButton qualityButton { "QUALITY" }, lowLatencyButton { "LOW LATENCY" };

    std::unique_ptr<SliderAttachment> pitchAtt, fineAtt, formantAtt, mixAtt, outputAtt;
    std::unique_ptr<ButtonAttachment> correctionAtt;
    std::unique_ptr<juce::ParameterAttachment> modeAtt;

    AmberDisplay display;
    VuMeter vu;
    juce::TooltipWindow tooltips { this, 700 };

    juce::Image faceplate;
    juce::Rectangle<int> header, formantGroup;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToneShiftEditor)
};
