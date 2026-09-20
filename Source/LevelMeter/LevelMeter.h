
#pragma once

#include <JuceHeader.h>
#include "../GonioMeter/Goniometer.h"
#include "../Constants.h"


struct Tick
{
    float db{ 0.f }; // dB value associated with the tick
    int y{ 0 }; // Y-coordinate of the tick on the scale
};

//==============================================================================
struct DbScale : juce::Component
{
    ~DbScale() override = default;

    void paint(juce::Graphics& g) override;

    // Builds the background image for the dB scale
    void buildBackgroundImage(int dbDivision, juce::Rectangle<int> meterBounds, int minDb, int maxDb);

    // Returns a vector of ticks for the dB scale
    static std::vector<Tick> getTicks(int dbDivision, juce::Rectangle<int> meterBounds, int minDb, int maxDb);

private:
    juce::Image bkgd; // Background image for the dB scale
    bool show_tick = true; // Flag indicating whether to display ticks on the scale
};

//==============================================================================
struct ValueHolder
{
    // Sets the threshold value
    void setThreshold(float th);

    // Updates the held value, elapsedSeconds is the time since the last update
    void updateHeldValue(float v, float elapsedSeconds = 0.f);

    // Sets the duration to hold the value
    void setHoldTime(int ms);

    // Returns the current value
    float getCurrentValue() const;

    // Returns the held value
    float getHeldValue() const;

    // Returns whether the current value is over the threshold
    bool getIsOverThreshold() const;

private:
    float threshold = 0; // Threshold value
    float currentValue = NEGATIVE_INFINITY; // Current value
    float heldValue = NEGATIVE_INFINITY; // Held value
    float secondsSincePeak = 0.f; // Time since the value was last over the threshold
    int durationToHoldForMs{ 500 }; // Duration to hold the value in milliseconds
    bool isOverThreshold{ false }; // Flag indicating whether the value is over the threshold
};

//==============================================================================
struct TextMeter : juce::Component
{
    TextMeter();

    // Paints the component
    void paint(juce::Graphics& g) override;

    // Updates the displayed dB value, elapsedSeconds is the time since the last update
    void update(float valueDb, float elapsedSeconds);

private:
    float cachedValueDb = NEGATIVE_INFINITY; // Cached dB value
    ValueHolder valueHolder; // Value holder for managing the displayed value
};


//==============================================================================
struct DecayingValueHolder
{
    // Updates the held value with the input value, then holds or decays it
    // according to the time since the last update
    void updateHeldValue(float input, float elapsedSeconds);

    // Returns the current value
    float getCurrentValue() const { return currentValue; };

    // Returns whether the current value is over the threshold
    bool isOverThreshold() const { return currentValue > threshold; };

    // Sets the hold time for the value, which can be infinite
    void setHoldTime(float seconds) { holdTimeSeconds = seconds; };

    // Sets the decay rate for the level meter
    void setLevelMeterDecay(float dbPerSec) { decayRatePerSecond = dbPerSec; };

    // Sets the current value
    void setCurrentValue(float val);

    // Returns the hold time in seconds
    float getHoldTime() const { return holdTimeSeconds; };

private:
    // Resets the decay multiplier for the level meter
    void resetLevelMeterDecayMultiplier() { decayRateMultiplier = 1; };

    float currentValue{ NEGATIVE_INFINITY }; // Current value
    float secondsSincePeak = 0.f; // Time since the peak value
    float threshold = 0.f; // Threshold value
    float holdTimeSeconds = 2.f; // Hold time in seconds (default: 2 seconds)
    float decayRatePerSecond{ 3.f }; // Decay rate in dB per second
    float decayRateMultiplier{ 1 }; // Decay rate multiplier
};

//==============================================================================
struct Meter : juce::Component
{
    // Paints the component
    void paint(juce::Graphics&) override;

    // Updates the meter with the specified dB level, decay rate, hold time in seconds, reset flag,
    // show tick flag, and the time since the last update
    void update(float dbLevel, float decay_rate, float hold_time_, bool reset_hold, bool show_tick_, float elapsedSeconds);

private:
    float peakDb { NEGATIVE_INFINITY }; // Peak dB level
    bool show_tick = false; // Flag indicating whether to show the tick
    DecayingValueHolder decayingValueHolder; // Decaying value holder for managing the meter value
};

//==============================================================================
class MacroMeter : public juce::Component
{
public:
    MacroMeter();

    // Called when the component is resized
    void resized() override;

    // Updates the macro meter with the specified parameters
    void update(float level, float decay_rate, bool show_peak, bool shwo_avg, float hold_time_, bool reset_hold, bool show_tick_, float elapsedSeconds);

private:
    TextMeter textMeter; // Text meter component
    Meter instantMeter, averageMeter; // Instant and average meter components
    Averager<float> averager; // Averager for managing the average level
    bool show_peak_ = true; // Flag indicating whether to show the peak level
    bool show_avg_ = true; // Flag indicating whether to show the average level
};

//==============================================================================
class StereoMeter : public juce::Component
{
public:
    // Constructor with name input
    StereoMeter(juce::String nameInput);

    // Paints the component
    void paint(juce::Graphics& g) override;

    // Called when the component is resized
    void resized() override;

    // Updates the stereo meter with left and right channel dB levels, decay rate, meter view ID, show tick flag,
    // hold time in seconds, reset hold flag, and the time since the last update
    void update(float leftChanDb, float rightChanDb, float decay_rate, int meterViewID, bool show_tick, float hold_time_, bool reset_hold, float elapsedSeconds);

    // Sets the text label
    void setText(juce::String labelName);

private:
    juce::Rectangle<int> labelTextArea; // Rectangle for label area
    juce::String labelText; // Text label
    MacroMeter leftMeter, rightMeter; // Left and right macro meter components
    DbScale dbScale; // dB scale component
    juce::Label label; // Label component
};
