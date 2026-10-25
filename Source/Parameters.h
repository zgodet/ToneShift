/*
    ToneShift by Audemo
    Parameter IDs and layout.
*/
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace audemo::params
{
    inline constexpr const char* pitch    = "pitch";     // semitones
    inline constexpr const char* fine     = "fine";      // cents
    inline constexpr const char* formant  = "formant";   // semitones
    inline constexpr const char* preserve = "preserve";  // formant correction on/off
    inline constexpr const char* mode     = "mode";      // 0 = Quality, 1 = Low Latency
    inline constexpr const char* mix      = "mix";       // percent
    inline constexpr const char* output   = "output";    // dB

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        AudioProcessorValueTreeState::ParameterLayout layout;

        auto semis = [] (float v, int) { return (v > 0.0f ? "+" : "") + String (v, 0) + " st"; };
        auto cents = [] (float v, int) { return (v > 0.0f ? "+" : "") + String (v, 0) + " ct"; };

        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pitch, 1 }, "Pitch",
            NormalisableRange<float> (-24.0f, 24.0f, 1.0f), 0.0f,
            AudioParameterFloatAttributes().withLabel ("st").withStringFromValueFunction (semis)));

        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { fine, 1 }, "Fine",
            NormalisableRange<float> (-100.0f, 100.0f, 1.0f), 0.0f,
            AudioParameterFloatAttributes().withLabel ("ct").withStringFromValueFunction (cents)));

        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { formant, 1 }, "Formant",
            NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
            AudioParameterFloatAttributes().withLabel ("st").withStringFromValueFunction (
                [] (float v, int) { return (v > 0.0f ? "+" : "") + String (v, 1) + " st"; })));

        layout.add (std::make_unique<AudioParameterBool> (ParameterID { preserve, 1 }, "Formant Correction", true));

        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { mode, 1 }, "Mode",
            StringArray { "Quality", "Low Latency" }, 0));

        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { mix, 1 }, "Mix",
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
            AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (
                [] (float v, int) { return String (v, 0) + " %"; })));

        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { output, 1 }, "Output",
            NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
            AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (
                [] (float v, int) { return (v > 0.0f ? "+" : "") + String (v, 1) + " dB"; })));

        return layout;
    }
}
