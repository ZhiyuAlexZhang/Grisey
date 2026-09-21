#pragma once

#include <JuceHeader.h>
#include "../UI/Theme.h"
#include "ScrollingImage.h"
#include "SpectrumSource.h"
#include "Timeline.h"

//==============================================================================
// A spectrogram that scrolls from right to left: time runs along the x axis, frequency up the
// y axis on a logarithmic scale, and the color shows the level. It draws the mid spectrum of
// the SpectrumSource. It is on the shared timeline, so it records whether it is showing or not.
class SpectrogramView : public juce::Component
{
public:
    // The levels that the darkest and the brightest colors stand for
    static constexpr float minDecibels = -96.f;
    static constexpr float maxDecibels = -6.f;

    // What is recorded for every slot: the level at this many frequencies, spaced logarithmically.
    // The picture is made from it at whatever height the view has.
    static constexpr int numRows = 256;

    explicit SpectrogramView(SpectrumSource&);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

    // Forgets what has been recorded
    void clearHistory();

    // Records the source's spectra, called by the editor once per frame, whichever view is showing.
    //  - numNewSlots:   how many slots of the timeline have been completed since the last call
    //  - hasNewSpectra: whether the source has analyzed new audio since the last call
    //  - frozen:        while frozen the picture stands still, but the recording carries on, so
    //                   that the picture is up to date again as soon as it is released
    void record(int numNewSlots, bool hasNewSpectra, float tiltDbPerOctave, bool frozen);

    // Sets how much time is shown
    void setSpan(float seconds);

private:
    // One slot: the level at each frequency, from the lowest to the highest, as an index into the colors
    using Column = std::array<juce::uint8, numRows>;

    float yOf(double frequency) const;

    // Adds the column of one slot at the right edge of the image
    void addColumn(const Column& column);

    // Makes the whole image again from what has been recorded
    void rebuildImage();

    SpectrumSource& source;
    juce::Rectangle<int> plot;
    float spanSeconds = 30.f;

    // What has been recorded, and the picture of the part of it that is shown, one column per slot
    Timeline::History<Column> history;
    ScrollingImage image;
    bool isFrozen = false;

    // The highest levels since the last slot was completed, in decibels
    std::array<float, numRows> pending;
    bool hasPending = false;

    // The last column that was recorded. A slot can pass without a new spectrum, when the host
    // delivers its audio in blocks that are longer than a slot, and it then shows this again
    // rather than a gap.
    Column lastColumn {};

    SpectrumEngine::Display display;
    std::vector<float> spectrum;
    std::array<juce::Colour, 256> colourTable;

    // The height of the mouse, if it is over the plot
    std::optional<int> hoverY;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrogramView)
};
