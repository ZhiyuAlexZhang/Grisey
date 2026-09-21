#pragma once

#include <JuceHeader.h>
#include "../UI/Theme.h"
#include "../Engine/LoudnessMeter.h"
#include "Timeline.h"

//==============================================================================
// Shows a history of the short-term and the momentary loudness, beneath the readings that
// only this view has: the momentary loudness, the PLR and the PSR. The other readings are in
// the side column, whichever view is showing, so they are not repeated here. The readings
// themselves are made on the audio thread, this component only displays them. The history is
// on the shared timeline, so it records whether the view is showing or not.
class LoudnessView : public juce::Component
{
public:
    // The graph reaches from full scale down, whatever the target is. It used to reach from 9 LU above
    // the target, like the EBU +9 scale, which a loud master went off the top of: with a target of -14
    // the top was -5, and the momentary loudness of a modern master goes above that.
    static constexpr float maxLufs = 0.f;
    static constexpr float minLufs = -48.f;
    static constexpr float gridStepLu = 6.f;

    LoudnessView();

    void paint(juce::Graphics& g) override;

    // Records the readings, called by the editor once per frame, whichever view is showing.
    //  - truePeakDb:    the higher channel's true peak since the last update
    //  - maxTruePeakDb: the higher channel's true peak since the last reset
    //  - targetLufs:    the loudness to aim for, or 0 for none
    //  - numNewSlots:   how many slots of the timeline have been completed since the last call
    //  - readoutDue:    whether the numbers are to take the new readings, which the editor decides
    //                   for every number at once
    void update(const LoudnessMeter::Readings& readings, float truePeakDb, float maxTruePeakDb,
                float targetLufs, int numNewSlots, float elapsedSeconds, bool readoutDue);

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

    void paintHeader(juce::Graphics& g, juce::Rectangle<int> area);
    void paintHistory(juce::Graphics& g, juce::Rectangle<int> area);

    // The room to the right of the plot for the labels of its scale
    static constexpr int scaleWidth = 34;

    LoudnessMeter::Readings readings;
    float maxTruePeak = -200.f;
    float target = 0.f;

    // What the numbers say: the readings as they were at the last readout. The side column takes
    // its readings at the same moments, so a PLR is always of the true peak and the integrated
    // loudness that are showing there.
    struct Readout
    {
        LoudnessMeter::Readings readings;
        float maxTruePeak = -200.f;
        float recentTruePeak = -200.f;
    };

    Readout shown;

    // The peak to short-term loudness ratio needs the true peak of the same 3 s as the short-term loudness.
    // The peaks are kept as the maximum of each 100 ms, so the 3 s hold however often the editor updates.
    static constexpr double truePeakSlotSeconds = 0.1;
    std::array<float, 30> recentTruePeaks;
    size_t recentTruePeakIndex = 0;
    double secondsInTruePeakSlot = 0.0;
    float recentTruePeak = -200.f;

    Timeline::History<HistoryPoint> history;
    float spanSeconds = 30.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoudnessView)
};
