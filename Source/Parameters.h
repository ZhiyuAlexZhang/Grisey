#pragma once

#include <JuceHeader.h>
#include <array>
#include <limits>

//==============================================================================
// Every setting of the plugin is a parameter in the AudioProcessorValueTreeState,
// so that it is saved with the session. The display settings are not automatable,
// which keeps them out of the host's automation lists.
namespace Parameters
{
    namespace ID
    {
        // "Scale Knob" is the ID that version 1 used, so it has to stay as it is
        inline const juce::String goniometerScale { "Scale Knob" };
        inline const juce::String decayRate { "decayRate" };
        inline const juce::String holdTime { "holdTime" };
        inline const juce::String averagerDuration { "averagerDuration" };
        inline const juce::String meterView { "meterView" };
        inline const juce::String showTick { "showTick" };
        inline const juce::String mainView { "mainView" };
        inline const juce::String goniometerMode { "goniometerMode" };
        inline const juce::String goniometerPersistence { "goniometerPersistence" };
        inline const juce::String spectrumChannels { "spectrumChannels" };
        inline const juce::String spectrumTilt { "spectrumTilt" };
        inline const juce::String spectrumSmoothing { "spectrumSmoothing" };
        inline const juce::String spectrumResolution { "spectrumResolution" };
        inline const juce::String spectrumPeakHold { "spectrumPeakHold" };
        inline const juce::String loudnessTarget { "loudnessTarget" };
        inline const juce::String refreshRate { "refreshRate" };
    }

    // The options of each choice parameter, and the values that they stand for
    inline const juce::StringArray decayRateNames { "-3dB/s", "-6dB/s", "-12dB/s", "-24dB/s", "-36dB/s" };
    inline constexpr std::array<float, 5> decayRatesDbPerSecond { 3.f, 6.f, 12.f, 24.f, 36.f };

    inline const juce::StringArray holdTimeNames { "0s", "0.5s", "2s", "4s", "6s", "inf" };
    inline constexpr std::array<float, 6> holdTimesSeconds { 0.f, 0.5f, 2.f, 4.f, 6.f, std::numeric_limits<float>::infinity() };

    inline const juce::StringArray averagerDurationNames { "100ms", "250ms", "500ms", "1000ms", "2000ms" };
    inline constexpr std::array<float, 5> averagerDurationsSeconds { 0.1f, 0.25f, 0.5f, 1.f, 2.f };

    inline const juce::StringArray meterViewNames { "Peak + RMS", "Peak", "RMS" };
    inline const juce::StringArray mainViewNames { "Goniometer", "Spectrum", "Spectrogram", "History", "Loudness" };

    enum MeterView
    {
        peakAndRmsMeters,
        peakMeters,
        rmsMeters
    };

    enum MainView
    {
        viewGoniometer,
        viewSpectrum,
        viewSpectrogram,
        viewHistory,
        viewLoudness
    };

    inline const juce::StringArray goniometerModeNames { "Lissajous", "Polar" };

    enum GoniometerMode
    {
        lissajousMode,
        polarMode
    };

    inline const juce::StringArray goniometerPersistenceNames { "Off", "Short", "Long" };
    inline constexpr std::array<float, 3> goniometerPersistenceSeconds { 0.f, 0.15f, 0.6f };

    inline const juce::StringArray spectrumChannelsNames { "Left / Right", "Mid / Side" };

    inline const juce::StringArray spectrumTiltNames { "0 dB/oct", "3 dB/oct", "4.5 dB/oct", "6 dB/oct" };
    inline constexpr std::array<float, 4> spectrumTiltsDbPerOctave { 0.f, 3.f, 4.5f, 6.f };

    inline const juce::StringArray spectrumSmoothingNames { "Off", "1/12 oct", "1/6 oct", "1/3 oct" };
    inline constexpr std::array<float, 4> spectrumSmoothingOctaves { 0.f, 1.f / 12.f, 1.f / 6.f, 1.f / 3.f };

    inline const juce::StringArray spectrumResolutionNames { "2048", "4096", "8192", "16384" };
    inline constexpr std::array<int, 4> spectrumResolutionOrders { 11, 12, 13, 14 };

    // The loudness that common platforms and standards ask for. 0 stands for no target.
    inline const juce::StringArray loudnessTargetNames { "Off", "-14 LUFS Streaming", "-16 LUFS Podcast", "-23 LUFS EBU R 128", "-24 LKFS ATSC A/85" };
    inline constexpr std::array<float, 5> loudnessTargetsLufs { 0.f, -14.f, -16.f, -23.f, -24.f };

    // How often the editor redraws. On macOS every frame costs a flush of the whole window, whatever
    // is drawn in it, so the frame rate is what decides how much of the CPU the editor takes.
    inline const juce::StringArray refreshRateNames { "30 fps", "60 fps" };
    inline constexpr std::array<int, 2> refreshRatesHz { 30, 60 };

    // Looks up the value of a choice parameter, whatever index it is given
    template<typename Table>
    auto valueAt(const Table& table, int index)
    {
        return table[static_cast<size_t>(juce::jlimit(0, static_cast<int>(table.size()) - 1, index))];
    }

    //==============================================================================
    // The saved state is the APVTS tree as XML, tagged with this version number.
    // Raise it whenever the meaning of a saved value changes, and migrate older
    // states in MultiMeterAudioProcessor::setStateInformation().
    inline constexpr int currentStateVersion = 2;
    inline const juce::Identifier stateVersionProperty { "stateVersion" };

    //==============================================================================
    // Version 1 saved its settings as a raw binary stream rather than as XML.
    // These are the settings it held, as parameter values.
    struct LegacyState
    {
        float goniometerScale = 100.f;
        int decayRate = 0, holdTime = 2, averagerDuration = 0, meterView = 0;
        bool showTick = true;
    };

    // The size of the version 1 stream: a float, two ints, a bool, and three ints
    inline constexpr int legacyStateSizeInBytes = 4 + 4 + 4 + 1 + 4 + 4 + 4;

    // Reads a version 1 state. Returns false if the data is not one.
    inline bool readLegacyState(const void* data, int sizeInBytes, LegacyState& state)
    {
        if (data == nullptr || sizeInBytes != legacyStateSizeInBytes)
            return false;

        juce::MemoryInputStream stream(data, static_cast<size_t>(sizeInBytes), false);

        const float scale = stream.readFloat();
        const int decayId = stream.readInt();
        const int holdId = stream.readInt();
        const bool tick = stream.readBool();
        const int averagerId = stream.readInt();
        const int meterViewId = stream.readInt();
        stream.readInt(); // how the two histograms were laid out, which the one history graph has no use for

        // Version 1 could save uninitialized values, so anything out of range
        // falls back to the default, as its editor did. The combo box IDs
        // started at 1, and the view IDs at 0.
        auto inRange = [](int value, int low, int high) { return value >= low && value <= high; };

        state.goniometerScale = (scale >= 50.f && scale <= 200.f) ? scale : 100.f;
        state.decayRate = inRange(decayId, 1, 5) ? decayId - 1 : 0;
        state.holdTime = inRange(holdId, 1, 6) ? holdId - 1 : 2;
        state.averagerDuration = inRange(averagerId, 1, 5) ? averagerId - 1 : 0;
        state.meterView = inRange(meterViewId, 0, 2) ? meterViewId : 0;
        state.showTick = tick;
        return true;
    }

    //==============================================================================
    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using Choice = juce::AudioParameterChoice;
        const auto display = juce::AudioParameterChoiceAttributes().withAutomatable(false);

        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        // Version hint 1 marks the parameter from version 1, and 2 the ones added in 2.0
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ID::goniometerScale, 1 },
            "Goniometer Scale",
            juce::NormalisableRange<float>(50.f, 200.f, 1.f, 0.1f),
            100.f));

        layout.add(std::make_unique<Choice>(juce::ParameterID { ID::decayRate, 2 }, "Tick Decay", decayRateNames, 0, display));
        layout.add(std::make_unique<Choice>(juce::ParameterID { ID::holdTime, 2 }, "Tick Hold", holdTimeNames, 2, display));
        layout.add(std::make_unique<Choice>(juce::ParameterID { ID::averagerDuration, 2 }, "Correlation Time", averagerDurationNames, 0, display));
        layout.add(std::make_unique<Choice>(juce::ParameterID { ID::meterView, 2 }, "Level Meters", meterViewNames, 0, display));
        layout.add(std::make_unique<Choice>(juce::ParameterID { ID::mainView, 2 }, "View", mainViewNames, viewSpectrum, display));

        layout.add(std::make_unique<Choice>(juce::ParameterID { ID::goniometerMode, 2 }, "Goniometer Mode", goniometerModeNames, lissajousMode, display));
        layout.add(std::make_unique<Choice>(juce::ParameterID { ID::goniometerPersistence, 2 }, "Goniometer Persistence", goniometerPersistenceNames, 1, display));
        layout.add(std::make_unique<Choice>(juce::ParameterID { ID::spectrumChannels, 2 }, "Spectrum Channels", spectrumChannelsNames, 0, display));
        layout.add(std::make_unique<Choice>(juce::ParameterID { ID::spectrumTilt, 2 }, "Spectrum Tilt", spectrumTiltNames, 2, display));
        layout.add(std::make_unique<Choice>(juce::ParameterID { ID::spectrumSmoothing, 2 }, "Spectrum Smoothing", spectrumSmoothingNames, 2, display));
        layout.add(std::make_unique<Choice>(juce::ParameterID { ID::spectrumResolution, 2 }, "Spectrum Resolution", spectrumResolutionNames, 1, display));
        layout.add(std::make_unique<Choice>(juce::ParameterID { ID::loudnessTarget, 2 }, "Loudness Target", loudnessTargetNames, 1, display));
        layout.add(std::make_unique<Choice>(juce::ParameterID { ID::refreshRate, 2 }, "Refresh Rate", refreshRateNames, 0, display));

        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { ID::spectrumPeakHold, 2 }, "Spectrum Peak Hold", false,
            juce::AudioParameterBoolAttributes().withAutomatable(false)));

        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { ID::showTick, 2 }, "Ticks", true,
            juce::AudioParameterBoolAttributes().withAutomatable(false)));

        return layout;
    }
}
