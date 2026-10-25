/*
    ToneShift by Audiemo
*/
#include "SpectralShifter.h"
#include <cmath>
#include <algorithm>

namespace audiemo
{

namespace
{
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    inline float wrapPhase (float x) noexcept
    {
        return x - twoPi * std::round (x / twoPi);
    }
}

void SpectralShifter::prepare (double newSampleRate, int numChannels)
{
    sampleRate = newSampleRate;

    // Keep the frame length roughly constant in time (~46 ms).
    fftOrder = sampleRate <= 50000.0 ? 11 : (sampleRate <= 100000.0 ? 12 : 13);
    fftSize  = 1 << fftOrder;
    hopSize  = fftSize / 4;
    numBins  = fftSize / 2 + 1;

    // Cepstral lifter: keeps envelope detail above ~1.5 ms quefrency out,
    // so harmonics of voices up to ~650 Hz are smoothed away.
    lifterCutoff = juce::jlimit (16, fftSize / 4, (int) std::round (sampleRate * 0.0015));

    fft = std::make_unique<juce::dsp::FFT> (fftOrder);

    window.resize ((size_t) fftSize);
    for (int n = 0; n < fftSize; ++n)
        window[(size_t) n] = 0.5f - 0.5f * std::cos (twoPi * (float) n / (float) fftSize);

    fftData.assign ((size_t) fftSize * 2, 0.0f);
    cepData.assign ((size_t) fftSize * 2, 0.0f);

    for (auto* v : { &mag, &phase, &omega, &envelope, &outMag, &outPhase })
        v->assign ((size_t) numBins, 0.0f);

    peaks.reserve ((size_t) numBins);
    regionLo.reserve ((size_t) numBins);
    regionHi.reserve ((size_t) numBins);

    // Overlap-add normalisation for Hann analysis * Hann synthesis.
    // Measure the FFT round-trip gain instead of assuming the backend's scaling.
    {
        std::fill (fftData.begin(), fftData.end(), 0.0f);
        fftData[1] = 1.0f;
        fft->performRealOnlyForwardTransform (fftData.data());
        fft->performRealOnlyInverseTransform (fftData.data());
        const float roundTrip = std::abs (fftData[1]);

        float windowSum = 0.0f;
        for (int n = 0; n < fftSize; ++n)
            windowSum += window[(size_t) n] * window[(size_t) n];

        const float overlap = windowSum / (float) hopSize; // 1.5 for Hann^2 at 4x
        olaScale = 1.0f / (overlap * (roundTrip > 0.0f ? roundTrip : 1.0f));
    }

    channels.assign ((size_t) juce::jmax (1, numChannels), Channel{});
    for (auto& ch : channels)
    {
        ch.input.assign ((size_t) fftSize, 0.0f);
        ch.output.assign ((size_t) fftSize, 0.0f);
        ch.lastPhase.assign ((size_t) numBins, 0.0f);
        ch.synthPhase.assign ((size_t) numBins, 0.0f);
    }

    reset();
}

void SpectralShifter::reset()
{
    for (auto& ch : channels)
    {
        std::fill (ch.input.begin(), ch.input.end(), 0.0f);
        std::fill (ch.output.begin(), ch.output.end(), 0.0f);
        std::fill (ch.lastPhase.begin(), ch.lastPhase.end(), 0.0f);
        std::fill (ch.synthPhase.begin(), ch.synthPhase.end(), 0.0f);
        ch.inputPos = ch.outputPos = ch.hopCounter = 0;
    }
}

void SpectralShifter::setParameters (float pitchRatio, float newFormantRatio, bool preserveFormants) noexcept
{
    ratio        = juce::jlimit (0.25f, 4.0f, pitchRatio);
    formantRatio = juce::jlimit (0.25f, 4.0f, newFormantRatio);
    preserve     = preserveFormants;
}

void SpectralShifter::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int numSamples = buffer.getNumSamples();
    const int numCh = juce::jmin (buffer.getNumChannels(), (int) channels.size());

    for (int c = 0; c < numCh; ++c)
    {
        auto& ch = channels[(size_t) c];
        float* data = buffer.getWritePointer (c);

        for (int i = 0; i < numSamples; ++i)
        {
            ch.input[(size_t) ch.inputPos] = data[i];
            ch.inputPos = (ch.inputPos + 1) % fftSize;

            data[i] = ch.output[(size_t) ch.outputPos++];

            if (++ch.hopCounter >= hopSize)
            {
                ch.hopCounter = 0;
                processFrame (ch);
            }
        }
    }
}

float SpectralShifter::envelopeAt (float bin) const noexcept
{
    if (bin <= 0.0f)
        return envelope[0];

    const int i = (int) bin;
    if (i >= numBins - 1)
        return envelope[(size_t) numBins - 1];

    const float f = bin - (float) i;
    return envelope[(size_t) i] + f * (envelope[(size_t) i + 1] - envelope[(size_t) i]);
}

void SpectralShifter::computeEnvelope() noexcept
{
    // Real cepstrum of the log-magnitude spectrum, low-quefrency lifter,
    // and back again. Both transforms act on symmetric real sequences,
    // so a forward real FFT does the job in each direction.
    std::fill (cepData.begin(), cepData.end(), 0.0f);
    for (int n = 0; n < fftSize; ++n)
    {
        const int k = n <= fftSize / 2 ? n : fftSize - n;
        cepData[(size_t) n] = std::log (mag[(size_t) k] + 1.0e-9f);
    }

    fft->performRealOnlyForwardTransform (cepData.data(), true);

    // Lifter (with a short raised-cosine taper to avoid ringing).
    const int taper = juce::jmax (2, lifterCutoff / 4);
    std::vector<float>& c = outPhase; // reuse as scratch: cepstral coefficients q = 0..numBins-1
    for (int q = 0; q < numBins; ++q)
    {
        float w = 0.0f;
        if (q <= lifterCutoff)
            w = 1.0f;
        else if (q < lifterCutoff + taper)
            w = 0.5f + 0.5f * std::cos (juce::MathConstants<float>::pi * (float) (q - lifterCutoff) / (float) taper);

        c[(size_t) q] = cepData[(size_t) q * 2] * w / (float) fftSize;
    }

    std::fill (cepData.begin(), cepData.end(), 0.0f);
    for (int n = 0; n < fftSize; ++n)
    {
        const int q = n <= fftSize / 2 ? n : fftSize - n;
        cepData[(size_t) n] = c[(size_t) q];
    }

    fft->performRealOnlyForwardTransform (cepData.data(), true);

    for (int k = 0; k < numBins; ++k)
        envelope[(size_t) k] = std::exp (juce::jlimit (-30.0f, 30.0f, cepData[(size_t) k * 2]));
}

void SpectralShifter::processFrame (Channel& ch) noexcept
{
    const int N = fftSize, H = hopSize, B = numBins;

    // ---- Analysis -------------------------------------------------------
    for (int n = 0; n < N; ++n)
        fftData[(size_t) n] = ch.input[(size_t) ((ch.inputPos + n) % N)] * window[(size_t) n];
    std::fill (fftData.begin() + N, fftData.end(), 0.0f);

    fft->performRealOnlyForwardTransform (fftData.data(), true);

    float maxMag = 0.0f;
    for (int k = 0; k < B; ++k)
    {
        const float re = fftData[(size_t) k * 2], im = fftData[(size_t) k * 2 + 1];
        const float m = std::sqrt (re * re + im * im);
        const float p = std::atan2 (im, re);

        mag[(size_t) k] = m;
        phase[(size_t) k] = p;
        maxMag = juce::jmax (maxMag, m);

        // Instantaneous frequency (radians / sample).
        const float binOmega = twoPi * (float) k / (float) N;
        const float dev = wrapPhase (p - ch.lastPhase[(size_t) k] - binOmega * (float) H);
        omega[(size_t) k] = binOmega + dev / (float) H;
        ch.lastPhase[(size_t) k] = p;
    }

    // ---- Formant envelope ---------------------------------------------
    const bool correctFormants = preserve && std::abs (formantRatio - ratio) > 1.0e-4f;
    if (correctFormants && maxMag > 0.0f)
        computeEnvelope();

    // ---- Peak picking and regions of influence ------------------------
    peaks.clear();
    const float threshold = maxMag * 1.0e-5f + 1.0e-12f;
    for (int k = 1; k < B - 1; ++k)
        if (mag[(size_t) k] > threshold && mag[(size_t) k] > mag[(size_t) k - 1] && mag[(size_t) k] >= mag[(size_t) k + 1])
            peaks.push_back (k);

    regionLo.resize (peaks.size());
    regionHi.resize (peaks.size());
    for (size_t i = 0; i < peaks.size(); ++i)
    {
        if (i == 0)
            regionLo[i] = 0;
        else
        {
            // Boundary at the magnitude minimum between neighbouring peaks.
            int lo = peaks[i - 1] + 1, best = lo;
            for (int k = lo; k < peaks[i]; ++k)
                if (mag[(size_t) k] < mag[(size_t) best])
                    best = k;
            regionLo[i] = best;
            regionHi[i - 1] = best - 1;
        }
    }
    if (! peaks.empty())
        regionHi.back() = B - 1;

    // ---- Shift peak regions rigidly, lock phases to their peak -------
    std::fill (outMag.begin(), outMag.end(), 0.0f);

    // Unwritten bins keep advancing at their centre frequency.
    for (int j = 0; j < B; ++j)
        outPhase[(size_t) j] = wrapPhase (ch.synthPhase[(size_t) j] + twoPi * (float) j * (float) H / (float) N);

    const float invRatio = 1.0f / ratio;
    const float invFormant = 1.0f / formantRatio;
    constexpr float maxCorrection = 8.0f; // ~ +18 dB

    for (size_t i = 0; i < peaks.size(); ++i)
    {
        const int p = peaks[i];
        const int jp = (int) std::lround ((float) p * ratio);
        if (jp < 1 || jp >= B - 1)
            continue;

        const int shift = jp - p;
        const float peakPhase = wrapPhase (ch.synthPhase[(size_t) jp] + (float) H * omega[(size_t) p] * ratio);

        for (int k = regionLo[i]; k <= regionHi[i]; ++k)
        {
            const int j = k + shift;
            if (j < 0 || j >= B)
                continue;

            float m = mag[(size_t) k];
            if (correctFormants)
            {
                const float target  = envelopeAt ((float) j * invFormant);
                const float carried = envelopeAt ((float) j * invRatio);
                m *= juce::jlimit (0.0f, maxCorrection, target / (carried + 1.0e-12f));
            }

            if (m > outMag[(size_t) j])
            {
                outMag[(size_t) j] = m;
                outPhase[(size_t) j] = wrapPhase (peakPhase + phase[(size_t) k] - phase[(size_t) p]);
            }
        }
    }

    std::copy (outPhase.begin(), outPhase.end(), ch.synthPhase.begin());

    // ---- Synthesis ----------------------------------------------------
    for (int j = 0; j < B; ++j)
    {
        fftData[(size_t) j * 2]     = outMag[(size_t) j] * std::cos (outPhase[(size_t) j]);
        fftData[(size_t) j * 2 + 1] = outMag[(size_t) j] * std::sin (outPhase[(size_t) j]);
    }
    fftData[1] = 0.0f;                    // DC is real
    fftData[(size_t) (B - 1) * 2 + 1] = 0.0f; // Nyquist is real
    for (int j = B; j < N; ++j)           // conjugate-symmetric upper half
    {
        fftData[(size_t) j * 2]     =  fftData[(size_t) (N - j) * 2];
        fftData[(size_t) j * 2 + 1] = -fftData[(size_t) (N - j) * 2 + 1];
    }

    fft->performRealOnlyInverseTransform (fftData.data());

    // ---- Overlap-add --------------------------------------------------
    // Drop the hop we just played, then add the new frame.
    std::copy (ch.output.begin() + H, ch.output.end(), ch.output.begin());
    std::fill (ch.output.end() - H, ch.output.end(), 0.0f);

    for (int n = 0; n < N; ++n)
        ch.output[(size_t) n] += fftData[(size_t) n] * window[(size_t) n] * olaScale;

    ch.outputPos = 0;
}

} // namespace audiemo
