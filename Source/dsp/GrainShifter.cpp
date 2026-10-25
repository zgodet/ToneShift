/*
    ToneShift by Audiemo
*/
#include "GrainShifter.h"
#include <cmath>

namespace audiemo
{

void GrainShifter::prepare (double sampleRate, int numChannels)
{
    const float ms = (float) sampleRate * 0.001f;

    window     = 24.0f * ms;
    search     = (int) (7.0f * ms);     // covers periods down to ~70 Hz
    corrLength = (int) (6.0f * ms);
    fadeLength = (int) (6.0f * ms);
    minDelay   = 4.0f;
    maxDelay   = minDelay + window + (float) (search + fadeLength);

    bufferSize = (int) juce::nextPowerOfTwo ((int) (maxDelay + window + (float) (search + corrLength) * 2.0f) + 64);
    mask = bufferSize - 1;

    lines.assign ((size_t) juce::jmax (1, numChannels), std::vector<float> ((size_t) bufferSize, 0.0f));
    mono.assign ((size_t) bufferSize, 0.0f);

    ratio.reset (sampleRate, 0.05);
    reset();
}

void GrainShifter::reset()
{
    for (auto& l : lines)
        std::fill (l.begin(), l.end(), 0.0f);
    std::fill (mono.begin(), mono.end(), 0.0f);

    writePos = 0;
    active = 0;
    fadePos = -1;
    delay[0] = delay[1] = 0.5f * (minDelay + maxDelay);
    ratio.setCurrentAndTargetValue (ratio.getTargetValue());
}

void GrainShifter::setPitchRatio (float newRatio) noexcept
{
    ratio.setTargetValue (juce::jlimit (0.25f, 4.0f, newRatio));
}

float GrainShifter::read (const std::vector<float>& line, float d) const noexcept
{
    const float pos = (float) writePos - d;
    const float fl = std::floor (pos);
    const float t = pos - fl;
    const int i = (int) fl;

    const float y0 = line[(size_t) ((i - 1) & mask)];
    const float y1 = line[(size_t) ((i)     & mask)];
    const float y2 = line[(size_t) ((i + 1) & mask)];
    const float y3 = line[(size_t) ((i + 2) & mask)];

    // Catmull-Rom
    const float a = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
    const float b = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c = -0.5f * y0 + 0.5f * y2;
    return ((a * t + b) * t + c) * t + y1;
}

float GrainShifter::findSpliceDelay (float fromDelay, float nominal) const noexcept
{
    // Compare the audio the current head has just played with the audio
    // just behind each candidate position, and pick the best normalised
    // correlation. Looking backwards keeps the delay (latency) small.
    const int ref = writePos - (int) std::lround (fromDelay) - corrLength;
    const int base = writePos - (int) std::lround (nominal) - corrLength;

    double refEnergy = 0.0;
    for (int n = 0; n < corrLength; n += 2)
        refEnergy += (double) monoAt (ref + n) * monoAt (ref + n);

    if (refEnergy < 1.0e-10)
        return nominal; // silence: alignment doesn't matter

    auto score = [&] (int offset, int step)
    {
        double xy = 0.0, yy = 0.0;
        const int cand = base - offset;
        for (int n = 0; n < corrLength; n += step)
        {
            const float y = monoAt (cand + n);
            xy += (double) monoAt (ref + n) * y;
            yy += (double) y * y;
        }
        return yy > 1.0e-12 ? xy / std::sqrt (yy) : -1.0e9;
    };

    // Coarse pass, then refine around the winner.
    int best = 0;
    double bestScore = -1.0e18;
    for (int off = -search; off <= search; off += 2)
    {
        const double s = score (off, 2);
        if (s > bestScore) { bestScore = s; best = off; }
    }
    const int coarse = best;
    for (int off = coarse - 2; off <= coarse + 2; ++off)
    {
        const double s = score (off, 1);
        if (off == coarse - 2 || s > bestScore) { bestScore = s; best = off; }
    }

    return nominal + (float) best;
}

void GrainShifter::startSplice (float r) noexcept
{
    const float current = delay[active];
    // Pitching up the head catches up with the input: jump back (more delay).
    // Pitching down it falls behind: jump forward (less delay).
    float nominal = r > 1.0f ? current + window : current - window;
    nominal = juce::jlimit (minDelay + (float) search, maxDelay - (float) search, nominal);

    const int next = 1 - active;
    delay[next] = findSpliceDelay (current, nominal);
    active = next;
    fadePos = 0;
}

void GrainShifter::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int numSamples = buffer.getNumSamples();
    const int numCh = juce::jmin (buffer.getNumChannels(), (int) lines.size());
    constexpr float pi = juce::MathConstants<float>::pi;

    for (int i = 0; i < numSamples; ++i)
    {
        // Write input
        float sum = 0.0f;
        for (int c = 0; c < numCh; ++c)
        {
            const float x = buffer.getSample (c, i);
            lines[(size_t) c][(size_t) writePos] = x;
            sum += x;
        }
        mono[(size_t) writePos] = sum;

        const float r = ratio.getNextValue();

        // Splice when the active head would leave the safe range during a fade.
        if (fadePos < 0)
        {
            const float drift = (1.0f - r) * (float) fadeLength;
            const float projected = delay[active] + drift;
            if (projected < minDelay || projected > maxDelay)
                startSplice (r);
        }

        // Read
        float gIn = 1.0f, gOut = 0.0f;
        if (fadePos >= 0)
        {
            const float t = (float) fadePos / (float) fadeLength;
            gIn = 0.5f - 0.5f * std::cos (pi * t);
            gOut = 1.0f - gIn;
        }

        const int other = 1 - active;
        for (int c = 0; c < numCh; ++c)
        {
            const auto& line = lines[(size_t) c];
            float y = gIn * read (line, delay[active]);
            if (fadePos >= 0)
                y += gOut * read (line, delay[other]);
            buffer.setSample (c, i, y);
        }

        // Advance heads
        delay[0] += 1.0f - r;
        delay[1] += 1.0f - r;
        delay[0] = juce::jlimit (2.0f, (float) bufferSize - 8.0f, delay[0]);
        delay[1] = juce::jlimit (2.0f, (float) bufferSize - 8.0f, delay[1]);

        if (fadePos >= 0 && ++fadePos >= fadeLength)
            fadePos = -1;

        writePos = (writePos + 1) & mask;
    }
}

} // namespace audiemo
