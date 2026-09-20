#pragma once

#include <JuceHeader.h>
#include "../Constants.h"
#include "../Engine/LoudnessMeter.h"

//==============================================================================
// Shows the readings of the LoudnessMeter and the TruePeakDetector as numbers, beside a
// history of the short-term and the momentary loudness. The readings themselves are made
// on the audio thread, this component only displays them.
struct LoudnessView : juce::Component
{
    // The history holds a point for every 100 ms, which is how often the loudness changes
    static constexpr double historyIntervalSeconds = 0.1;
    static constexpr int historyLength = 600;

    // The graph reaches from 27 LU below the target to 9 LU above it, like the EBU +9 scale
    static constexpr float rangeBelowTarget = 27.f;
    static constexpr float rangeAboveTarget = 9.f;

    // Most platforms ask for true peaks no higher than this
    static constexpr float truePeakLimitDb = -1.f;

    LoudnessView();

    void paint(juce::Graphics& g) override;

    // Updates the display, called by the editor once per frame.
    //  - truePeakDb:    the higher channel's true peak since the last update
    //  - maxTruePeakDb: the higher channel's true peak since the last reset
    //  - targetLufs:    the loudness to aim for, or 0 for none
    //  - audioRunning:  whether audio is arriving, the history stands still when it is not
    void update(const LoudnessMeter::Readings& readings, float truePeakDb, float maxTruePeakDb,
                float targetLufs, bool audioRunning, float elapsedSeconds);

    // Forgets the history, for when the loudness meter is reset
    void clearHistory();

private:
    struct HistoryPoint
    {
        float shortTerm = LoudnessMeter::silence;
        float momentary = LoudnessMeter::silence;
    };

    void paintReadouts(juce::Graphics& g, juce::Rectangle<int> area);
    void paintHistory(juce::Graphics& g, juce::Rectangle<int> area);

    // The loudness at the middle of the graph's scale
    float getReferenceLufs() const;

    LoudnessMeter::Readings readings;
    float maxTruePeak = NEGATIVE_INFINITY;
    float target = 0.f;

    // The peak to short-term loudness ratio needs the true peak of the same 3 s as the short-term loudness
    std::array<float, 180> recentTruePeaks;
    size_t recentTruePeakIndex = 0;
    float recentTruePeak = NEGATIVE_INFINITY;

    std::array<HistoryPoint, historyLength> history;
    int historyIndex = 0;
    double secondsSinceHistoryPoint = 0.0;
};
