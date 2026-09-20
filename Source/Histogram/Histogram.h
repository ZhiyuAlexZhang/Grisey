
#pragma once

#include <JuceHeader.h>
#include "../Constants.h"

using namespace juce;

//==============================================================================
// Circular buffer structure that allows reading all elements after writing
template<typename T>
struct ReadAllAfterWriteCircularBuffer
{
    using DataType = std::vector<T>; // Define the data type as a vector of type T

    // Constructor initializes the circular buffer with a specified fill value
    ReadAllAfterWriteCircularBuffer(T fillValue)
    {
        resize(1, fillValue); // Initialize with a single element and fill value
    }

    // Resize the circular buffer and initialize with the fill value
    void resize(std::size_t s, T fillValue)
    {
        data.resize(s); // Resize the data vector
        clear(fillValue); // Clear the data with the fill value
    }

    // Clear the circular buffer with the fill value
    void clear(T fillValue)
    {
        data.assign(getSize(), fillValue); // Assign fill value to all elements
        resetWriteIndex(); // Reset the write index to the beginning
    }

    // Write a new element to the circular buffer
    void write(T t)
    {
        if (getSize() == 0) // Nothing can be stored before the component has a width
            return;

        auto indexCopy = writeIndex.load(); // Load the current write index
        data[indexCopy] = t; // Write the element at the current index
        ++indexCopy; // Increment the index
        if (indexCopy == getSize()) // If index exceeds buffer size, wrap around
        {
            indexCopy = 0;
        }
        writeIndex.store(indexCopy); // Store the updated write index
    }

    // Get a reference to the underlying data vector
    DataType& getData()
    {
        return data;
    }

    // Get the read index (same as write index)
    size_t getReadIndex() const
    {
        return writeIndex.load();
    }

    // Get the size of the circular buffer
    size_t getSize() const
    {
        return data.size();
    }

private:
    // Reset the write index to the beginning
    void resetWriteIndex()
    {
        writeIndex.store(0);
    }

    std::atomic<std::size_t> writeIndex {0}; // Atomic variable for the write index
    DataType data; // Data vector for storing elements
};

//==============================================================================
// Structure representing a histogram component
struct Histogram : juce::Component
{
    // Constructor initializing the histogram with a title
    Histogram(const juce::String& titleInput);

    // Function to paint the histogram
    void paint(juce::Graphics& g) override;

    // Function to handle resizing of the histogram
    void resized() override;

    // Function to handle mouse down events on the histogram
    void mouseDown(const juce::MouseEvent& e) override;

    // Function to update the histogram with a new value
    void update(float value);

private:
    // Function to display the path on the graphics context within the specified bounds
    void displayPath(juce::Graphics& g, juce::Rectangle<float> bounds);

    // Function to build the path using the circular buffer data and bounds
    static juce::Path buildPath(juce::Path& p,
        ReadAllAfterWriteCircularBuffer<float>& buffer,
        juce::Rectangle<float> bounds)
    {
        p.clear(); // Clear the path
        if (buffer.getSize() == 0)
            return {};

        auto bufferSizeCopy = buffer.getSize(); // Get a copy of buffer size
        auto bottomOfBoundsCopy = bounds.getBottom(); // Get the bottom of the bounds
        auto& bufferDataCopy = buffer.getData(); // Get a reference to buffer data
        int readIndexCopy = static_cast<int>(buffer.getReadIndex()); // Get the read index

        // Lambda function to map decibel values to y-coordinates within the bounds
        auto map = [bottomOfBoundsCopy](float db) { return juce::jmap(db, NEGATIVE_INFINITY, MAX_DECIBELS, bottomOfBoundsCopy, 0.f); };
        // Lambda function to increment index with wrap-around behavior
        auto increment = [bufferSizeCopy](int& index) mutable { index = (index + 1) % bufferSizeCopy; };

        // Start the path at the initial point
        p.startNewSubPath(0, map(bufferDataCopy[readIndexCopy]));
        increment(readIndexCopy); // Increment the read index

        // Build the path by iterating over the width of the bounds
        for (int i = 1; i < bounds.getWidth(); ++i)
        {
            p.lineTo(i, map(bufferDataCopy[readIndexCopy])); // Add a line segment to the path
            increment(readIndexCopy); // Increment the read index
        }

        juce::Path fillPath = p; // Create a copy of the path for filling
        if (p.getBounds().getHeight() > 0) // If path height is positive
        {
            // Complete the fill path by closing the subpath
            fillPath.lineTo(bounds.getRight(), bottomOfBoundsCopy);
            fillPath.lineTo(bounds.getX(), bottomOfBoundsCopy);
            fillPath.closeSubPath();
            return fillPath; // Return the filled path
        }
        return {}; // Return an empty path if height is not positive
    }

    // Circular buffer to store decibel values
    ReadAllAfterWriteCircularBuffer<float> buffer {float(NEGATIVE_INFINITY)};
    juce::Path path; // Path to display the waveform
    const juce::String title; // Title of the histogram
};
