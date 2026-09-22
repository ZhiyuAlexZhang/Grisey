#pragma once

#include <JuceHeader.h>
#include "../UI/Theme.h"
#include "../UI/CachedLayer.h"
#include "../UI/Controls.h"
#include "../Engine/SampleRingBuffer.h"

//==============================================================================
// Plots the left channel against the right, to show the width and the phase of the stereo image.
//
// The trace is kept as a grid of light intensities, like the phosphor of an oscilloscope: every
// sample adds light where the beam passes, and all of it fades with time. The beam has a constant
// power, so it is dimmer where it moves fast, and the shape of the signal stands out from its
// outliers. How long the light lingers is the persistence.
class GoniometerView : public juce::Component
{
public:
    enum Mode
    {
        // Mid runs up and down, and side runs left and right, so that a mono signal is a vertical
        // line, and each of the channels alone is a diagonal
        lissajous,

        // The same plot folded into its upper half, which only tells signals apart by their
        // balance and their phase: in phase between the diagonals, and out of phase outside them
        polar
    };

    // No more samples than this are plotted in one update
    static constexpr int maxSamplesPerUpdate = 4096;

    GoniometerView(juce::AudioProcessorValueTreeState& apvts, const juce::String& scaleParameterID);

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Plots the samples that have arrived since the last update, called by the editor once per frame.
    // A scale of 1 puts a full-scale mono signal on the edge of the circle.
    void update(const SampleRingBuffer& ringBuffer, float elapsedSeconds, Mode newMode, float persistenceSeconds, float newScale);

private:
    void paintBackground(juce::Graphics& g);

    // Adds the light of a beam that moves from one point of the grid to another
    void addLine(juce::Point<float> from, juce::Point<float> to);

    // Adds light to one cell of the grid
    void addLight(int x, int y, float amount);

    // Turns the grid of intensities into the image that paint() draws
    void renderImage();

    // The samples to plot
    juce::AudioBuffer<float> samples;

    // The ring buffer's sample count at the last update, which tells how many samples are new
    juce::uint64 lastTotalWritten = 0;

    // Where the beam was at the end of the last update, so that the trace carries on from there
    juce::Point<float> lastPoint;
    bool hasLastPoint = false;

    // The light in every cell of the grid, and the image made from it. The grid has two cells per
    // pixel each way, up to a limit, so that the trace stays sharp on a high-resolution display.
    static constexpr int cellsPerPixel = 2;
    static constexpr int maxGridSize = 560;
    std::vector<float> intensity;
    int gridSize = 0;
    juce::Image image;

    // Whether any cell of the grid holds light. While none does, and no samples arrive,
    // there is nothing to fade and nothing to draw.
    bool isLit = false;

    // How much light a sample adds at two cells per pixel: spread along its path in a line plot, and in
    // one cell in a dot plot. A coarser grid has fewer cells to share the light, so lightScale makes up
    // for it, and the trace is as bright whatever the size of the grid.
    static constexpr float lineLight = 3.f;
    static constexpr float dotLight = 0.3f;
    float lightScale = 1.f;

    // Light below this is too faint to change a pixel
    static constexpr float faintestLight = 0.002f;

    // The color of every brightness, worked out once. The brightness saturates as phosphor does,
    // so that dense parts of the trace keep their detail, and the brightest parts turn towards white.
    static constexpr int toneMapSize = 1024;
    static constexpr float toneMapMaxIntensity = 8.f;
    static constexpr float toneMapExposure = 3.5f;
    std::array<juce::PixelARGB, toneMapSize> toneMap;

    Mode mode = lissajous;
    float scale = 1.f;

    // The square that the plot is drawn in
    juce::Rectangle<int> plot;
    CachedLayer background;
    FloatingKnob scaleKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GoniometerView)
};
