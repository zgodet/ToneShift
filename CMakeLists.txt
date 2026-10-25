cmake_minimum_required(VERSION 3.22)

project(ToneShift VERSION 1.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# JUCE: use a local checkout if TONESHIFT_JUCE_PATH is given, else download it.
# ---------------------------------------------------------------------------
set(TONESHIFT_JUCE_PATH "" CACHE PATH "Path to a local JUCE checkout (optional)")

if(TONESHIFT_JUCE_PATH)
    add_subdirectory(${TONESHIFT_JUCE_PATH} JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------------------
# Plugin
# ---------------------------------------------------------------------------
set(TONESHIFT_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND TONESHIFT_FORMATS AU)
endif()
if(UNIX AND NOT APPLE)
    list(APPEND TONESHIFT_FORMATS LV2)
endif()

juce_add_plugin(ToneShift
    COMPANY_NAME                "Audemo"
    COMPANY_WEBSITE             "https://audemo.com"
    BUNDLE_ID                   "com.audemo.toneshift"
    PLUGIN_MANUFACTURER_CODE    Adem
    PLUGIN_CODE                 Tsft
    PRODUCT_NAME                "ToneShift"
    DESCRIPTION                 "Pitch shifter with formant correction"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Pitch Shift
    AU_MAIN_TYPE                kAudioUnitType_Effect
    LV2URI                      "https://audemo.com/plugins/toneshift"
    FORMATS                     ${TONESHIFT_FORMATS})

target_sources(ToneShift PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/dsp/SpectralShifter.cpp
    Source/dsp/GrainShifter.cpp)

target_include_directories(ToneShift PRIVATE Source)

target_compile_definitions(ToneShift PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(ToneShift
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Offline DSP tests
# ---------------------------------------------------------------------------
option(TONESHIFT_BUILD_TESTS "Build the DSP test runner" ON)

if(TONESHIFT_BUILD_TESTS)
    juce_add_console_app(ToneShiftTests PRODUCT_NAME "ToneShiftTests")
    target_sources(ToneShiftTests PRIVATE
        tests/DspTests.cpp
        Source/dsp/SpectralShifter.cpp
        Source/dsp/GrainShifter.cpp)
    target_include_directories(ToneShiftTests PRIVATE Source)
    target_compile_definitions(ToneShiftTests PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(ToneShiftTests PRIVATE
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
endif()
