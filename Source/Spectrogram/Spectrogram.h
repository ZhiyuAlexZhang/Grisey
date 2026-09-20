#pragma once

#include <JuceHeader.h>
#include "../Constants.h"
#include "../SpectrumAnalyzer/SpectrumSource.h"

//==============================================================================
// A spectrogram that scrolls from right to left: time runs along the x axis, frequency
// up the y axis on a logarithmic scale, and the color shows the level. It draws the
// mid spectrum of the SpectrumSource, one column for every analysis.
struct Spectrogram : juce::Component
{
    // The levels that the darkest and the brightest colors stand for
    static constexpr float minDecibels = -96.f;
    static constexpr float maxDecibels = -6.f;

    explicit Spectrogram(SpectrumSource&);

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Clicking the spectrogram clears it
    void mouseDown(const juce::MouseEvent& e) override;

    // Adds a column for the source's newest spectra, called by the editor after every new analysis
    void addColumn(float tiltDbPerOctave);

private:
    // The area that the image is drawn in
    juce::Rectangle<int> getImageArea() const;

    // The color of a level, from a table that is built once
    juce::Colour colourFor(float decibels) const;

    SpectrumSource& source;

    // The columns are written round and round the image, and paint() draws it in two parts,
    // so that adding a column never moves the pixels that are already there
    juce::Image image;
    int writeX = 0;

    SpectrumEngine::Display display;
    std::vector<float> column;
    std::array<juce::Colour, 256> colourTable;
};
