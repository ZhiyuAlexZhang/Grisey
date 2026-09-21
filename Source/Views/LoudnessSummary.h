#pragma once

#include <JuceHeader.h>
#include "../UI/Theme.h"
#include "../Engine/LoudnessMeter.h"

//==============================================================================
// The loudness readings that matter most, as numbers in the side column, where they
// can be seen whichever view is showing. The Loudness view has all of them.
class LoudnessSummary : public juce::Component
{
public:
    // Most platforms ask for true peaks no higher than this
    static constexpr float truePeakLimitDb = -1.f;

    LoudnessSummary() { setOpaque(true); }

    void paint(juce::Graphics& g) override;

    // Shows new readings, called by the editor once per frame. The numbers only take the new
    // readings when a readout is due, which the editor decides for every number at once, and
    // are only drawn again when what they say has changed. targetLufs is 0 for no target.
    void update(const LoudnessMeter::Readings& readings, float maxTruePeakDb, float targetLufs, bool readoutDue);

private:
    juce::String integrated, difference, shortTerm, range, truePeak;
    bool truePeakIsOver = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoudnessSummary)
};

//==============================================================================
// The correlation of left and right as a bar from the center: to the right and blue when the
// channels are in phase, to the left and red when they are out of phase. The bar is the slow
// reading, and the line over it is the fast one.
class CorrelationBar : public juce::Component
{
public:
    CorrelationBar() { setOpaque(true); }

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Shows new readings, called by the editor once per frame
    void update(float fastCorrelation, float slowCorrelation);

private:
    float xOf(float correlation) const;

    float fast = 0.f, slow = 0.f;
    juce::Rectangle<float> track;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CorrelationBar)
};
