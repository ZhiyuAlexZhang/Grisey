
#include "CorrelationMeter.h"

//==============================================================================
// Implementation for the CorrelationMeter class
void CorrelationMeter::paint(juce::Graphics& g)
{
    // Fill the background with the base color
    g.setColour(BASE_COLOR);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 3);

    // Divide the area into two parts: slowBounds and fastBounds
    auto slowBounds = getLocalBounds().removeFromTop(getLocalBounds().getHeight() / 3);

    // Draw the fast correlation reading in the top strip with a border
    drawAverage(g, slowBounds, fastCorrelation, true);

    // Draw the slow correlation reading over the whole meter with a border
    drawAverage(g, getLocalBounds(), slowCorrelation, true);

    // Draw the border around the component
    Path border;
    border.setUsingNonZeroWinding(false);
    border.addRectangle(getLocalBounds());
    auto bounds = getLocalBounds().toFloat().reduced(1);
    border.addRoundedRectangle(bounds, 3);
    g.setColour(BACKGROUND_COLOR);
    g.fillPath(border);
}

void CorrelationMeter::update(float newFastCorrelation, float newSlowCorrelation)
{
    // Repaint the component only when a reading has changed
    if (! juce::exactlyEqual(fastCorrelation, newFastCorrelation) || ! juce::exactlyEqual(slowCorrelation, newSlowCorrelation))
    {
        fastCorrelation = newFastCorrelation;
        slowCorrelation = newSlowCorrelation;
        repaint();
    }
}

void CorrelationMeter::drawAverage(juce::Graphics& g, juce::Rectangle<int> bounds, float avg, bool drawBorder)
{
    // Map the average value to the width of the bounds
    int width = juce::jmap(avg, -1.0f, 1.0f, 0.f, (float)bounds.getWidth());

    // Create a rectangle representing the average value
    juce::Rectangle<int> rect;
    if (avg >= 0)
    {
        rect.setBounds(bounds.getWidth() / 2, 0, width - bounds.getWidth() / 2, bounds.getHeight());
    }
    else
    {
        rect.setBounds(width, 0, bounds.getWidth() / 2 - width, bounds.getHeight());
    }

    // Fill the rectangle with the highlight color
    g.setColour(HIGHLIGHT_COLOR);
    g.fillRect(rect);

    // Draw a rounded rectangle border around the bounds if required
    if (drawBorder)
    {
        g.setColour(BACKGROUND_COLOR);
        g.drawRoundedRectangle(bounds.toFloat(), 3, 1);
    }
}
