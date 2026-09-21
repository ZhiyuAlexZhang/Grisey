#pragma once

#include <JuceHeader.h>
#include "../UI/Theme.h"
#include "ScrollingImage.h"
#include "SpectrumSource.h"

//==============================================================================
// A spectrogram that scrolls from right to left: time runs along the x axis, frequency up the
// y axis on a logarithmic scale, and the color shows the level. It draws the mid spectrum of
// the SpectrumSource, one column for every analysis.
class SpectrogramView : public juce::Component
{
public:
    // The levels that the darkest and the brightest colors stand for
    static constexpr float minDecibels = -96.f;
    static constexpr float maxDecibels = -6.f;

    explicit SpectrogramView(SpectrumSource&);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

    // Clicking the spectrogram clears it
    void mouseDown(const juce::MouseEvent&) override;

    // Adds a column for the source's newest spectra, called by the editor after every new analysis
    void addColumn(float tiltDbPerOctave);

private:
    float yOf(double frequency) const;

    SpectrumSource& source;
    juce::Rectangle<int> plot;
    ScrollingImage image;

    SpectrumEngine::Display display;
    std::vector<float> column;
    std::array<juce::Colour, 256> colourTable;

    // The height of the mouse, if it is over the plot
    std::optional<int> hoverY;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrogramView)
};
