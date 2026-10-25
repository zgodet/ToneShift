/*
    ToneShift by Audemo
    GrainShifter: low-latency time-domain pitch shifter. A read head runs
    through a short delay line at the pitch ratio; when it drifts out of
    range it is spliced to a new position, chosen by cross-correlation so
    the waveform lines up, and the two heads are crossfaded.
*/
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace audemo
{

class GrainShifter
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset();

    void setPitchRatio (float newRatio) noexcept;

    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    float read (const std::vector<float>& line, float delay) const noexcept;
    float monoAt (int index) const noexcept { return mono[(size_t) (index & mask)]; }
    float findSpliceDelay (float fromDelay, float nominalDelay) const noexcept;
    void startSplice (float r) noexcept;

    std::vector<std::vector<float>> lines;
    std::vector<float> mono;            // channel sum, used for alignment
    int bufferSize = 0, mask = 0, writePos = 0;

    // Timing (samples)
    float window = 1152.0f;             // nominal splice distance
    float minDelay = 4.0f, maxDelay = 1780.0f;
    int search = 336, corrLength = 288, fadeLength = 288;

    float delay[2] { 600.0f, 600.0f };
    int active = 0;                     // tap currently heard
    int fadePos = -1;                   // >= 0 while crossfading to `active`

    juce::SmoothedValue<float> ratio { 1.0f };
};

} // namespace audemo
