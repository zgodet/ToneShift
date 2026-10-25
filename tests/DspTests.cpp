/*
    ToneShift by Audemo - offline DSP checks.
    Runs each engine on test signals and checks pitch accuracy and transparency.
*/
#include <juce_dsp/juce_dsp.h>
#include "dsp/SpectralShifter.h"
#include "dsp/GrainShifter.h"
#include <cstdio>
#include <cmath>

using namespace audemo;

static constexpr double fs = 48000.0;
static int failures = 0;

static void check (bool ok, const char* what, double got, double want)
{
    std::printf ("[%s] %-48s got %10.3f  want %10.3f\n", ok ? " OK " : "FAIL", what, got, want);
    if (! ok) ++failures;
}

static juce::AudioBuffer<float> sine (double freq, int len, int channels = 2)
{
    juce::AudioBuffer<float> b (channels, len);
    for (int c = 0; c < channels; ++c)
        for (int i = 0; i < len; ++i)
            b.setSample (c, i, 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * freq * i / fs));
    return b;
}

template <typename Engine>
static void runInBlocks (Engine& e, juce::AudioBuffer<float>& b, int block = 512)
{
    for (int start = 0; start < b.getNumSamples(); start += block)
    {
        const int n = juce::jmin (block, b.getNumSamples() - start);
        juce::AudioBuffer<float> view (b.getArrayOfWritePointers(), b.getNumChannels(), start, n);
        e.process (view);
    }
}

// Frequency of the strongest component, with parabolic interpolation.
static double dominantFrequency (const juce::AudioBuffer<float>& b, int start)
{
    constexpr int order = 15, n = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> d ((size_t) n * 2, 0.0f);
    for (int i = 0; i < n; ++i)
    {
        const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * i / n);
        d[(size_t) i] = b.getSample (0, start + i) * w;
    }
    fft.performFrequencyOnlyForwardTransform (d.data(), true);
    int best = 1;
    for (int k = 1; k < n / 2 - 1; ++k)
        if (d[(size_t) k] > d[(size_t) best]) best = k;
    const double a = std::log (d[(size_t) best - 1] + 1e-12), m = std::log (d[(size_t) best] + 1e-12), c = std::log (d[(size_t) best + 1] + 1e-12);
    const double off = 0.5 * (a - c) / (a - 2 * m + c);
    return (best + off) * fs / n;
}

// Average frequency from upward zero crossings (robust to crossfade sidebands).
static double zeroCrossingFrequency (const juce::AudioBuffer<float>& b, int start, int len)
{
    int first = -1, last = -1, count = 0;
    for (int i = start + 1; i < start + len; ++i)
        if (b.getSample (0, i - 1) <= 0.0f && b.getSample (0, i) > 0.0f)
        {
            if (first < 0) first = i; else ++count;
            last = i;
        }
    return count > 0 ? count * fs / (last - first) : 0.0;
}

static double rms (const juce::AudioBuffer<float>& b, int start, int len)
{
    double s = 0;
    for (int i = 0; i < len; ++i) s += b.getSample (0, start + i) * b.getSample (0, start + i);
    return std::sqrt (s / len);
}

static bool allFinite (const juce::AudioBuffer<float>& b)
{
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (b.getSample (c, i))) return false;
    return true;
}

static float ratioFor (double semis) { return (float) std::pow (2.0, semis / 12.0); }

int main()
{
    const int len = (int) fs * 2;
    const int analyseFrom = (int) fs / 2;

    // ---------------- Spectral engine ----------------
    {
        SpectralShifter s;
        s.prepare (fs, 2);
        const int lat = s.getLatencySamples();

        // Transparency at unity ratio: output ~= input delayed by latency.
        auto in = sine (440.0, len);
        auto out = in;
        s.setParameters (1.0f, 1.0f, false);
        runInBlocks (s, out, 333);
        double err = 0, ref = 0;
        for (int i = analyseFrom; i < len; ++i)
        {
            const double e = out.getSample (0, i) - in.getSample (0, i - lat);
            err += e * e; ref += in.getSample (0, i - lat) * in.getSample (0, i - lat);
        }
        const double snr = 10 * std::log10 (ref / (err + 1e-20));
        check (snr > 40.0, "spectral: unity ratio SNR (dB) vs delayed input", snr, 40.0);

        const double tests[] = { 7.0, -12.0, 12.0, -5.0, 0.37, 24.0, -24.0 };
        for (double st : tests)
        {
            s.reset();
            auto b = sine (440.0, len);
            s.setParameters (ratioFor (st), 1.0f, false);
            runInBlocks (s, b);
            const double want = 440.0 * ratioFor (st);
            const double got = dominantFrequency (b, analyseFrom);
            char name[96]; std::snprintf (name, sizeof name, "spectral: 440 Hz %+.2f st (Hz)", st);
            check (std::abs (got - want) / want < 0.003, name, got, want);
            const double level = rms (b, analyseFrom, (int) fs) / rms (sine (440.0, len), analyseFrom, (int) fs);
            std::snprintf (name, sizeof name, "spectral: %+.2f st level ratio", st);
            check (level > 0.7 && level < 1.3, name, level, 1.0);
        }

        // Formant correction path: harmonic tone, check it stays sane.
        s.reset();
        juce::AudioBuffer<float> h (2, len);
        h.clear();
        for (int k = 1; k <= 30; ++k)
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < len; ++i)
                    h.addSample (c, i, (0.3f / k) * (float) std::sin (juce::MathConstants<double>::twoPi * 150.0 * k * i / fs));
        const double inRms = rms (h, analyseFrom, (int) fs);
        s.setParameters (ratioFor (5.0), 1.0f, true);
        runInBlocks (s, h);
        check (allFinite (h), "spectral: formant mode output is finite", 1, 1);
        const double fr = rms (h, analyseFrom, (int) fs) / inRms;
        check (fr > 0.4 && fr < 2.0, "spectral: formant mode level ratio", fr, 1.0);
    }

    // ---------------- Grain engine ----------------
    {
        GrainShifter g;
        g.prepare (fs, 2);
        const double tests[] = { 7.0, -12.0, 12.0, 3.0 };
        for (double st : tests)
        {
            g.reset();
            auto b = sine (440.0, len);
            g.setPitchRatio (ratioFor (st));
            g.reset(); // jump straight to target ratio
            runInBlocks (g, b);
            const double want = 440.0 * ratioFor (st);
            const double got = zeroCrossingFrequency (b, analyseFrom, (int) fs);
            char name[96]; std::snprintf (name, sizeof name, "grain: 440 Hz %+.2f st (Hz)", st);
            check (std::abs (got - want) / want < 0.005, name, got, want);
        }
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILURES", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
