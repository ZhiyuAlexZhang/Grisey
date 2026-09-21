#pragma once

#include <JuceHeader.h>
#include "../UI/Theme.h"
#include "../Engine/LoudnessMeter.h"
#include "Timeline.h"

//==============================================================================
// Shows the readings of the LoudnessMeter and the TruePeakDetector as numbers, beside a
// history of the short-term and the momentary loudness. The readings themselves are made
// on the audio thread, this component only displays them. The history is on the shared
// timeline, so it records whether the view is showing or not.
class LoudnessView : public juce::Component
{
public:
    // The graph reaches from 27 LU below the target to 9 LU above it, like the EBU +9 scale
    static constexpr float rangeBelowTarget = 27.f;
    static constexpr float rangeAboveTarget = 9.f;

    // Most platforms ask for true peaks no higher than this
    static constexpr float truePeakLimitDb = -1.f;

    LoudnessView();

    void paint(juce::Graphics& g) override;

    // Records the readings, called by the editor once per frame, whichever view is showing.
    //  - truePeakDb:    the higher channel's true peak since the last update
    //  - maxTruePeakDb: the higher channel's true peak since the last reset
    //  - targetLufs:    the loudness to aim for, or 0 for none
    //  - numNewSlots:   how many slots of the timeline have been completed since the last call
    void update(const LoudnessMeter::Readings& readings, float truePeakDb, float maxTruePeakDb,
                float targetLufs, int numNewSlots, float elapsedSeconds);

    // Forgets the history, for when the loudness meter is reset
    void clearHistory();

    // Sets how much time is shown
    void setSpan(float seconds);

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
    float maxTruePeak = -200.f;
    float target = 0.f;

    // The peak to short-term loudness ratio needs the true peak of the same 3 s as the short-term loudness.
    // The peaks are kept as the maximum of each 100 ms, so the 3 s hold however often the editor updates.
    static constexpr double truePeakSlotSeconds = 0.1;
    std::array<float, 30> recentTruePeaks;
    size_t recentTruePeakIndex = 0;
    double secondsInTruePeakSlot = 0.0;
    float recentTruePeak = -200.f;

    Timeline::History<HistoryPoint> history;
    float spanSeconds = 30.f;

    // The view is drawn again ten times a second, which is how often its readings change
    double secondsSinceRepaint = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoudnessView)
};
