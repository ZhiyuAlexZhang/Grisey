
#pragma once
#include <JuceHeader.h>
#include "../Constants.h"
#include "../Engine/SampleRingBuffer.h"

//==============================================================================
template<typename T>
struct Averager
{
    // Constructor initializes the averager with a specified number of elements and initial value
    Averager(size_t numElements, T initialValue)
    {
        resize(numElements, initialValue);
        init_size = numElements;
    }

    // Resizes the averager with a new number of elements and initial value
    void resize(size_t numElements, T initialValue)
    {
        elements.resize(numElements);
        clear(initialValue);
    }

    // Clears the averager and sets all elements to the initial value
    void clear(T initialValue)
    {
        elements.assign(getSize(), initialValue);
        writeIndex.store(0);
        sum.store(initialValue * getSize());
        avg.store(initialValue);
    }

    // Returns the size of the averager
    size_t getSize() const
    {
        return elements.size();
    }

    // Adds a new element to the averager
    void add(T t)
    {
        auto writeIndexCopy = writeIndex.load();
        auto sumCopy = sum.load();

        sumCopy -= elements[writeIndex];
        sumCopy += t;
        elements[writeIndex] = t;
        ++writeIndexCopy;
        if (writeIndexCopy == getSize())
        {
            writeIndexCopy = 0;
        }

        writeIndex.store(writeIndexCopy);
        sum.store(sumCopy);
        avg = static_cast<float>(sumCopy) / static_cast<float>(getSize());
    }

    // Returns the average value of the averager
    float getAvg() const
    {
        return avg.load();
    }

    // Sets the duration of the averager (in milliseconds)
    void setAveragerDuration(juce::int64 duration)
    {
        size_t new_size = static_cast<size_t>(init_size * duration / DEFAULT_SAMPLE_INTERVAL_MS);
        resize(new_size, static_cast<T>(getAvg()));
    }

private:
    std::vector<T> elements; // Buffer to store elements for averaging
    std::atomic<float> avg { static_cast<float>(T()) }; // Atomic variable for storing the average
    std::atomic<size_t> writeIndex = { 0 }; // Atomic variable for the write index
    std::atomic<T> sum { 0 }; // Atomic variable for the sum of elements
    size_t init_size = 0; // Initial size of the averager
    static constexpr int DEFAULT_SAMPLE_INTERVAL_MS = 100; // Default sample interval in milliseconds
};

//==============================================================================
// Plots the left channel against the right, to show the width and the phase of the stereo image.
//
// The trace is kept as a grid of light intensities, like the phosphor of an oscilloscope: every
// sample adds light where the beam passes, and all of it fades with time. The beam has a constant
// power, so it is dimmer where it moves fast, and the shape of the signal stands out from its
// outliers. How long the light lingers is the persistence.
struct Goniometer : juce::Component
{
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

    // Constructor
    Goniometer();

    // Paint method override
    void paint(juce::Graphics& g) override;

    // Resized method override
    void resized() override;

    // Plots the samples that have arrived since the last update, called by the editor once per frame
    void update(const SampleRingBuffer& ringBuffer, float elapsedSeconds, Mode newMode, float persistenceSeconds);

    // Method to update the visualization scaling coefficient
    void updateCoeff(float new_db);

private:
    // Helper method to draw the background
    void drawBackground(juce::Graphics& g);

    // Adds the light of a beam that moves from one point of the grid to another
    void addLine(juce::Point<float> from, juce::Point<float> to);

    // Adds light to one cell of the grid
    void addLight(int x, int y, float amount);

    // Turns the grid of intensities into the image that paint() draws
    void renderImage();

    // The radius of the plot in pixels
    float getRadius() const { return 0.5f * (float)w; }

    // The samples to plot
    juce::AudioBuffer<float> internalBuffer;

    // The ring buffer's sample count at the last update, which tells how many samples are new
    juce::uint64 lastTotalWritten = 0;

    // Where the beam was at the end of the last update, so that the trace carries on from there
    juce::Point<float> lastPoint;
    bool hasLastPoint = false;

    // The light in every cell of the grid, and the image made from it. The grid has
    // two cells per pixel each way, so that the trace stays sharp on a high-resolution display.
    static constexpr int cellsPerPixel = 2;

    // How much light a sample adds: spread along its path in a line plot, and in one cell in a dot plot
    static constexpr float lineLight = 3.f;
    static constexpr float dotLight = 0.3f;
    std::vector<float> intensity;
    int gridSize = 0;
    juce::Image image;

    Mode mode = lissajous;

    // Width and height of the component
    int w = 0, h = 0;

    // Center point of the component
    juce::Point<int> center;

    // Scaling factor for the visualization
    float scale;

    // Colors for edge and trace
    juce::Colour edgeColour { 0xffd2d2d2 };
    juce::Colour traceColour { 0xff48bde8 };
};
