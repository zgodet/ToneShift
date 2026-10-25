/*
    ToneShift by Audiemo
*/
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/SpectralShifter.h"
#include "dsp/GrainShifter.h"

class ToneShiftProcessor final : public juce::AudioProcessor
{
public:
    ToneShiftProcessor();
    ~ToneShiftProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    /** Peak levels for the editor meters (written on the audio thread). */
    std::atomic<float> inputLevel { 0.0f }, outputLevel { 0.0f };

private:
    void updateLatency (int mode);
    void delayDry (juce::AudioBuffer<float>& dry, int delaySamples);

    audiemo::SpectralShifter spectral;
    audiemo::GrainShifter grain;

    juce::AudioBuffer<float> dryBuffer, dryDelay;
    int dryDelayPos = 0, currentMode = -1, currentLatency = 0;

    juce::SmoothedValue<float> mixSmoothed, gainSmoothed;

    std::atomic<float>* pitchParam = nullptr;
    std::atomic<float>* fineParam = nullptr;
    std::atomic<float>* formantParam = nullptr;
    std::atomic<float>* preserveParam = nullptr;
    std::atomic<float>* modeParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* outputParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToneShiftProcessor)
};
