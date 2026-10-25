/*
    ToneShift by Audemo
    SpectralShifter: phase-vocoder pitch shifter with peak-locked phases
    and cepstral formant correction.
*/
#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>

namespace audemo
{

class SpectralShifter
{
public:
    SpectralShifter() = default;

    /** Allocates everything. Call from prepareToPlay, not the audio thread. */
    void prepare (double sampleRate, int numChannels);
    void reset();

    /** Latency in samples introduced by the analysis frame. */
    int getLatencySamples() const noexcept { return fftSize; }

    /** pitchRatio: frequency multiplier (0.25 .. 4).
        formantRatio: how far the spectral envelope moves (only used when
        preserveFormants is true; 1 = keep original formants). */
    void setParameters (float pitchRatio, float formantRatio, bool preserveFormants) noexcept;

    /** Processes the buffer in place. Channel count must not exceed prepare(). */
    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    struct Channel
    {
        std::vector<float> input;      // circular, fftSize
        std::vector<float> output;     // overlap-add accumulator, fftSize
        std::vector<float> lastPhase;  // analysis phase of previous frame
        std::vector<float> synthPhase; // synthesis phase of previous frame
        int inputPos   = 0;
        int outputPos  = 0;
        int hopCounter = 0;
    };

    void processFrame (Channel& ch) noexcept;
    void computeEnvelope() noexcept;
    float envelopeAt (float bin) const noexcept;

    double sampleRate = 44100.0;
    int fftOrder = 11, fftSize = 2048, hopSize = 512, numBins = 1025;
    int lifterCutoff = 64;
    float olaScale = 1.0f;

    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window, fftData, cepData;
    std::vector<float> mag, phase, omega, envelope;
    std::vector<float> outMag, outPhase;
    std::vector<int> peaks, regionLo, regionHi;
    std::vector<Channel> channels;

    float ratio = 1.0f, formantRatio = 1.0f;
    bool preserve = false;
};

} // namespace audemo
