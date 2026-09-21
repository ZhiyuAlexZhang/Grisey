#pragma once

#include <JuceHeader.h>
#include "../UI/Theme.h"
#include "ScrollingImage.h"

//==============================================================================
// The level over the last half minute, scrolling from right to left: the RMS level as a solid
// shape, and the peak level as a lighter shape above it. It takes the louder of the two channels.
class HistoryView : public juce::Component
{
public:
    static constexpr float maxDb = 6.f;
    static constexpr float minDb = -60.f;
    static constexpr double windowSeconds = 30.0;

    HistoryView() { setOpaque(true); }

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Clicking the history clears it
    void mouseDown(const juce::MouseEvent&) override;

    // Records the levels, called by the editor once per frame, whichever view is showing
    void update(float peakDb, float rmsDb, float elapsedSeconds);

private:
    float yOf(float decibels) const;

    juce::Rectangle<int> plot;
    ScrollingImage image;

    // The colors of a column from the top down: within the RMS shape, within the peak shape, and outside both
    std::vector<juce::Colour> rmsColours, peakColours;

    // A column covers more than one update, so it shows the highest levels of its time
    double secondsInColumn = 0.0;
    float columnPeak = -200.f, columnRms = -200.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HistoryView)
};
