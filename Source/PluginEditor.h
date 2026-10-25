/*
    ToneShift by Audiemo
*/
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "gui/AudiemoLookAndFeel.h"

/** Horizontal semitone scale. Click or drag to pick a pitch. */
class PitchRuler final : public juce::Component,
                         public juce::SettableTooltipClient
{
public:
    PitchRuler (juce::RangedAudioParameter& pitch, juce::RangedAudioParameter& fine);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    float xForSemitone (float st) const;
    void setFromX (float x);

    juce::RangedAudioParameter& pitchParam;
    juce::RangedAudioParameter& fineParam;
    juce::ParameterAttachment pitchAttachment, fineAttachment;
};

/** Small vertical peak meter. */
class LevelMeter final : public juce::Component
{
public:
    explicit LevelMeter (juce::String labelText) : label (std::move (labelText)) {}
    void setLevel (float newLevel);
    void paint (juce::Graphics&) override;

private:
    juce::String label;
    float level = 0.0f;
};

class ToneShiftEditor final : public juce::AudioProcessorEditor,
                              private juce::Timer
{
public:
    explicit ToneShiftEditor (ToneShiftProcessor&);
    ~ToneShiftEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void setupKnob (juce::Slider&, juce::Label&, const juce::String& name, bool bipolar, double resetValue);
    void updateFormantEnablement();
    void drawSection (juce::Graphics&, juce::Rectangle<int>, const juce::String& title);

    ToneShiftProcessor& processor;
    audiemo::AudiemoLookAndFeel lnf;

    juce::Slider pitch, fine, formant, mix, output;
    juce::Label pitchLabel, fineLabel, formantLabel, mixLabel, outputLabel;
    juce::ToggleButton correction { "CORRECTION" };
    juce::TextButton qualityButton { "QUALITY" }, lowLatencyButton { "LOW LATENCY" };

    std::unique_ptr<SliderAttachment> pitchAtt, fineAtt, formantAtt, mixAtt, outputAtt;
    std::unique_ptr<ButtonAttachment> correctionAtt;
    std::unique_ptr<juce::ParameterAttachment> modeAtt;

    PitchRuler ruler;
    LevelMeter inMeter { "IN" }, outMeter { "OUT" };
    juce::TooltipWindow tooltips { this, 700 };

    juce::Rectangle<int> fineArea, formantArea, pitchArea, outputArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToneShiftEditor)
};
