
#pragma once
#include <JuceHeader.h>
#include "../Constants.h"

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
struct Goniometer : juce::Component
{
    // The number of most recent samples that are plotted in every frame. At
    // 60 frames per second this covers every sample up to a 61.4 kHz sample rate
    static constexpr int numSamplesToPlot = 1024;

    // Constructor
    Goniometer();

    // Paint method override
    void paint(juce::Graphics& g) override;

    // Resized method override
    void resized() override;

    // Returns the stereo buffer to fill with the samples to plot, before calling repaint()
    juce::AudioBuffer<float>& getBuffer() { return internalBuffer; }

    // Method to update the visualization scaling coefficient
    void updateCoeff(float new_db);

private:
    // Helper method to draw the background
    void drawBackground(juce::Graphics& g);

    // The samples to plot
    juce::AudioBuffer<float> internalBuffer;

    // Path for drawing the visualization
    juce::Path p;

    // Width and height of the component
    int w, h;

    // Center point of the component
    juce::Point<int> center;

    // Labels for different regions
    std::vector<juce::String> chars { "+S", "-S", "L", "M", "R" };

    // Scaling factor for the visualization
    float scale;

    // Colors for edge, inside, and outside paths
    juce::Colour edgeColour { 0xffd2d2d2 };
    juce::Colour pathColourInside { 0xffd2d2d2 };
    juce::Colour pathColourOutside { 0xff48bde8 };
};
