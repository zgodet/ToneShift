/*
    ToneShift by Audemo
*/
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"

using namespace audemo;

ToneShiftProcessor::ToneShiftProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ToneShiftState", params::createLayout())
{
    pitchParam    = apvts.getRawParameterValue (params::pitch);
    fineParam     = apvts.getRawParameterValue (params::fine);
    formantParam  = apvts.getRawParameterValue (params::formant);
    preserveParam = apvts.getRawParameterValue (params::preserve);
    modeParam     = apvts.getRawParameterValue (params::mode);
    mixParam      = apvts.getRawParameterValue (params::mix);
    outputParam   = apvts.getRawParameterValue (params::output);
}

bool ToneShiftProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void ToneShiftProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int numCh = juce::jmax (getTotalNumInputChannels(), getTotalNumOutputChannels());

    spectral.prepare (sampleRate, numCh);
    grain.prepare (sampleRate, numCh);

    dryBuffer.setSize (numCh, samplesPerBlock);
    dryDelay.setSize (numCh, spectral.getLatencySamples() + 1);
    dryDelay.clear();
    dryDelayPos = 0;

    mixSmoothed.reset (sampleRate, 0.03);
    gainSmoothed.reset (sampleRate, 0.03);
    mixSmoothed.setCurrentAndTargetValue (mixParam->load() * 0.01f);
    gainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outputParam->load()));

    currentMode = -1;
    updateLatency ((int) modeParam->load());
}

void ToneShiftProcessor::updateLatency (int mode)
{
    if (mode == currentMode)
        return;

    currentMode = mode;
    currentLatency = mode == 0 ? spectral.getLatencySamples() : 0;
    setLatencySamples (currentLatency);

    spectral.reset();
    grain.reset();
    dryDelay.clear();
    dryDelayPos = 0;
}

void ToneShiftProcessor::delayDry (juce::AudioBuffer<float>& dry, int delaySamples)
{
    if (delaySamples <= 0)
        return;

    const int size = dryDelay.getNumSamples();
    const int numCh = juce::jmin (dry.getNumChannels(), dryDelay.getNumChannels());
    int pos = dryDelayPos;

    for (int i = 0; i < dry.getNumSamples(); ++i)
    {
        const int readPos = (pos - delaySamples + size) % size;
        for (int c = 0; c < numCh; ++c)
        {
            auto* line = dryDelay.getWritePointer (c);
            auto* d = dry.getWritePointer (c);
            line[pos] = d[i];
            d[i] = line[readPos];
        }
        pos = (pos + 1) % size;
    }
    dryDelayPos = pos;
}

void ToneShiftProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numCh = getTotalNumOutputChannels();

    for (int c = getTotalNumInputChannels(); c < numCh; ++c)
        buffer.clear (c, 0, numSamples);

    inputLevel.store (juce::jmax (inputLevel.load() * 0.9f, buffer.getMagnitude (0, numSamples)));

    // ---- Parameters ------------------------------------------------------
    const int mode = (int) modeParam->load();
    updateLatency (mode);

    const float semis = pitchParam->load() + fineParam->load() * 0.01f;
    const float ratio = std::pow (2.0f, semis / 12.0f);
    const bool preserve = preserveParam->load() > 0.5f;
    const float formantRatio = std::pow (2.0f, formantParam->load() / 12.0f);

    mixSmoothed.setTargetValue (mixParam->load() * 0.01f);
    gainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (outputParam->load()));

    // ---- Dry copy, aligned with the wet path -----------------------------
    if (dryBuffer.getNumSamples() < numSamples || dryBuffer.getNumChannels() < numCh)
        dryBuffer.setSize (numCh, numSamples, false, false, true); // only if the host lied about block size

    for (int c = 0; c < numCh; ++c)
        dryBuffer.copyFrom (c, 0, buffer, c, 0, numSamples);

    juce::AudioBuffer<float> dry (dryBuffer.getArrayOfWritePointers(), numCh, numSamples);
    delayDry (dry, currentLatency);

    // ---- Wet --------------------------------------------------------------
    if (mode == 0)
    {
        spectral.setParameters (ratio, formantRatio, preserve);
        spectral.process (buffer);
    }
    else
    {
        grain.setPitchRatio (ratio);
        grain.process (buffer);
    }

    // ---- Mix and output gain ---------------------------------------------
    for (int i = 0; i < numSamples; ++i)
    {
        const float wet = mixSmoothed.getNextValue();
        const float gain = gainSmoothed.getNextValue();
        for (int c = 0; c < numCh; ++c)
        {
            auto* out = buffer.getWritePointer (c);
            out[i] = gain * (wet * out[i] + (1.0f - wet) * dry.getSample (c, i));
        }
    }

    outputLevel.store (juce::jmax (outputLevel.load() * 0.9f, buffer.getMagnitude (0, numSamples)));
}

void ToneShiftProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ToneShiftProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* ToneShiftProcessor::createEditor()
{
    return new ToneShiftEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ToneShiftProcessor();
}
