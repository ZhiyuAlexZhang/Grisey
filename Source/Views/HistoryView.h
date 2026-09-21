#pragma once

#include <JuceHeader.h>
#include "../UI/Theme.h"
#include "ScrollingImage.h"
#include "Timeline.h"

//==============================================================================
// The level over time, scrolling from right to left: the RMS level as a solid shape, and the peak
// level as a lighter shape above it. It takes the louder of the two channels. It is on the shared
// timeline, so it records whether it is showing or not.
class HistoryView : public juce::Component
{
public:
    static constexpr float maxDb = 6.f;
    static constexpr float minDb = -60.f;

    HistoryView() { setOpaque(true); }

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Clicking RMS or PEAK in the corner asks for that shape to be shown or hidden.
    // Clicking the history itself clears it.
    void mouseDown(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;

    // Which of the shapes to draw. The editor sets this from the setting that the level bars
    // also follow, so that the bars and their history show the same.
    void setShown(bool peak, bool rms);

    // Called when RMS or PEAK has been clicked, with what should be shown from now on.
    // One of the two is always shown: hiding the only one that is showing brings back the other.
    std::function<void(bool showPeak, bool showRms)> onShownClicked;

    // Records the levels, called by the editor once per frame, whichever view is showing.
    // numNewSlots is how many slots of the timeline have been completed since the last call.
    void record(int numNewSlots, float peakDb, float rmsDb);

    // Sets how much time is shown
    void setSpan(float seconds);

private:
    struct Levels
    {
        float peak = -200.f, rms = -200.f;
    };

    float yOf(float decibels) const;

    // Adds the column of one slot at the right edge of the image
    void addColumn(const Levels& levels);

    // Makes the whole image again from what has been recorded
    void rebuildImage();

    juce::Rectangle<int> plot, rmsLabel, peakLabel;
    float spanSeconds = 30.f;
    bool showPeak = true, showRms = true;

    // What has been recorded, and the picture of the part of it that is shown, one column per slot
    Timeline::History<Levels> history;
    ScrollingImage image;

    // The highest levels since the last slot was completed
    Levels pending;

    // The colors of a column from the top down: within the RMS shape, within the peak shape, and outside
    // both, where it is the background of the display, which is not the same color all the way down.
    // The peak shape is faint behind the RMS shape, and takes the solid colors when it is on its own.
    std::vector<juce::Colour> rmsColours, peakColours, backgroundColours;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HistoryView)
};
