#include "Goniometer.h"

//==============================================================================
// Implementation for the Goniometer class
Goniometer::Goniometer(juce::AudioBuffer<float>& bufferInput) : buffer(bufferInput)
{
    // Initialize the internal buffer with the same size as the input buffer and clear it
    internalBuffer.setSize(2, bufferInput.getNumSamples(), false, true, true);
    internalBuffer.clear();
    // Initialize the scaling factor
    scale = 1;
}

void Goniometer::paint(juce::Graphics& g)
{
    // Draw the background of the goniometer
    drawBackground(g);
    // Clear the path for drawing the visualization
    p.clear();

    // Copy the audio data from the input buffer to the internal buffer
    for (int channel = 0; channel < 2; ++channel) // Assuming stereo data (2 channels)
    {
        internalBuffer.copyFrom(channel, 0, buffer, channel, 0, internalBuffer.getNumSamples());
    }

    // Get the size of the internal buffer
    int internalBufferSize = internalBuffer.getNumSamples();

    // Calculate the scaling coefficient based on the scale factor
    float coefficient = juce::Decibels::decibelsToGain(0.f + juce::Decibels::gainToDecibels(scale));
    
    // Calculate the maximum and minimum gain values
    float maxGain = juce::Decibels::decibelsToGain(MAX_DECIBELS);
    float minGain = juce::Decibels::decibelsToGain(NEGATIVE_INFINITY);
    
    // Iterate over each sample in the internal buffer
    for (int i = 0; i < internalBufferSize; ++i)
    {
        // Calculate the S and M values for each sample
        float leftRaw = internalBuffer.getSample(0, i);
        float rightRaw = internalBuffer.getSample(1, i);
        float S = (leftRaw - rightRaw) * coefficient;
        float M = (leftRaw + rightRaw) * coefficient;
        
        // Map the S and M values to screen coordinates
        auto a = (float)getLocalBounds().getX() + getWidth() / 2;
        auto b = (float)getLocalBounds().getRight() + getWidth() / 2 - 40;
        auto c = (float)getLocalBounds().getBottom() - getHeight() / 2 - 40;
        auto d = (float)getLocalBounds().getY() - getHeight() / 2;
        float xCoordinate = juce::jmap(S, minGain, maxGain, (float)a, (float)b);
        float yCoordinate = juce::jmap(M, minGain, maxGain, (float)c, (float)d);
        
        // Create a point representing the current sample in screen coordinates
        juce::Point<float> point{xCoordinate, yCoordinate};
        
        // Start a new sub-path if it's the first sample, otherwise add a line segment to the path
        if (i == 0)
        {
            p.startNewSubPath(point);
        }
        if (point.isFinite())
        {
            p.lineTo(point);
        }
    }

    // Draw the path if it's not empty
    if (!p.isEmpty())
    {
        // Flip the path vertically
        p.applyTransform(juce::AffineTransform::verticalFlip(h));
        // Create a gradient fill for the path
        juce::ColourGradient gradientColor(pathColourInside, center.x, center.y,
            pathColourOutside, w / 2, h / 2, true);
        g.setGradientFill(gradientColor);
        // Stroke the path with the gradient fill
        g.strokePath(p, juce::PathStrokeType(1));
    }
}

void Goniometer::resized()
{
    // Update the center point of the component based on its new dimensions
    center = juce::Point<int>(getWidth() / 2, getHeight() / 2);
    // Update the width and height variables used for drawing the background
    w = getWidth() - 40;
    h = getHeight() - 40;
}

void Goniometer::drawBackground(juce::Graphics& g)
{
    // Draw the background ellipse with the edge color
    g.setColour(edgeColour);
    g.drawEllipse(center.getX() - w / 2, center.getY() - h / 2, w, h, 1);
    // Fill the background ellipse with the base color
    g.setColour(BASE_COLOR);
    g.fillEllipse(center.getX() - w / 2, center.getY() - h / 2, w, h);

    // Draw the radial lines and labels
    for (int i = 0; i < 8; ++i)
    {
        // Calculate the end point of each radial line
        juce::Point<float> endPoint = center.getPointOnCircumference(122, i * juce::MathConstants<double>::pi / 4 + juce::MathConstants<double>::pi / 2);
        // Draw the radial line
        g.setColour(juce::Colours::grey);
        g.drawLine(juce::Line<float>(center.toFloat(), endPoint.toFloat()), 1);
        // Draw the label for each region
        if (endPoint.getY() <= center.getY())
        {
            int additionalDistanceY = 0;
            int additionalDistanceX = 0;
            if (i == 6)
            {
                additionalDistanceY = -15;
            }
            else if (i == 5 || i == 7)
            {
                additionalDistanceX = (i == 5 ? -12 : 12);
                additionalDistanceY = -15;
            }
            else if (i == 4 || i == 0)
            {
                additionalDistanceX = (i == 4 ? -10 : 10);
                additionalDistanceY = -7;
            }
            // Draw the label with the base color
            g.setColour(BASE_COLOR);
            g.drawText(chars[i == 0 ? i : i - 3],
                endPoint.getX() - 10 + additionalDistanceX,
                endPoint.getY() + additionalDistanceY,
                20,
                10,
                juce::Justification::centredTop);
        }
    }
}

void Goniometer::update(juce::AudioBuffer<float>& bufferInput) 
{
    // Ensure internalBuffer matches the size and content of bufferInput
    if (internalBuffer.getNumSamples() != bufferInput.getNumSamples() || internalBuffer.getNumChannels() != bufferInput.getNumChannels())
    {
        internalBuffer.setSize(bufferInput.getNumChannels(), bufferInput.getNumSamples(), true);
    }
    
    for (int channel = 0; channel < internalBuffer.getNumChannels(); ++channel) 
    {
        internalBuffer.copyFrom(channel, 0, bufferInput, channel, 0, bufferInput.getNumSamples());
    }
}

void Goniometer::updateCoeff(float new_db)
{
    // Update the scaling coefficient with the new dB value
    scale = new_db;
}
