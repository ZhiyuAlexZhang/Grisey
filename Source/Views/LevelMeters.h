#pragma once

#include <JuceHeader.h>
#include "../UI/Theme.h"
#include "../UI/CachedLayer.h"
#include "../Engine/LoudnessMeter.h"

//==============================================================================
// The bar meters of the side column, on one scale from +6 down to -60:
//
//  - Left and right: the RMS level as a solid bar, the peak level as a lighter bar above it,
//    and a tick that holds the highest peak for a while before it falls
//  - Loudness: the momentary loudness as a thin bar, the short-term loudness as a wide one,
//    a line at the integrated loudness, and a marker at the target
//
// Above each channel is its highest peak since the meters were last clicked.
class LevelMeters : public juce::Component
{
public:
    static constexpr float maxDb = 6.f;
    static constexpr float minDb = -60.f;

    // How fast the peak bars fall, so that a peak can be seen. The ticks have their own setting.
    static constexpr float peakReleaseDbPerSecond = 24.f;

    struct Settings
    {
        float tickDecayDbPerSecond = 3.f; // How fast the ticks fall once their hold has passed
        float tickHoldSeconds = 2.f;      // How long the ticks are held, which can be infinite
        bool showPeak = true, showRms = true, showTicks = true;
        bool resetTicks = false;          // True for the one update after a reset was asked for
    };

    struct Levels
    {
        std::array<float, 2> peakDb { -200.f, -200.f };
        std::array<float, 2> rmsDb { -200.f, -200.f };
        float momentaryLufs = LoudnessMeter::silence;
        float shortTermLufs = LoudnessMeter::silence;
        float integratedLufs = LoudnessMeter::silence;
        float targetLufs = 0.f; // 0 for none
    };

    LevelMeters();

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Clicking the meters restarts the highest peaks and the ticks
    void mouseDown(const juce::MouseEvent&) override;

    // Moves the meters to the new levels, called by the editor once per frame
    void update(const Levels& levels, const Settings& settings, float elapsedSeconds);

private:
    struct Channel
    {
        float peak = -200.f, rms = -200.f;   // what the bars show
        float tick = -200.f;                 // the held peak
        float secondsSinceTick = 0.f;
        float tickDecayMultiplier = 1.f;
        float highest = -200.f;              // the highest peak since the last click
    };

    // The vertical position of a level within the bars
    float yOf(float decibels) const;

    void paintStaticLayer(juce::Graphics& g);
    void paintChannel(juce::Graphics& g, const Channel& channel, juce::Rectangle<float> bar) const;

    std::array<Channel, 2> channels;
    Levels shown;
    Settings settings;
    double secondsSinceReadout = 0.0;
    std::array<juce::String, 2> readouts;

    // Layout
    juce::Rectangle<int> barsArea;
    std::array<juce::Rectangle<float>, 2> channelBars;
    juce::Rectangle<float> momentaryBar, shortTermBar;
    std::array<juce::Rectangle<int>, 2> readoutAreas;

    // The colors of a bar run up its height, so that a level always has the same color
    juce::ColourGradient levelGradient, loudnessGradient;
    CachedLayer staticLayer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeters)
};
