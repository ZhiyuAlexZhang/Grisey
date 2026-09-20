
#pragma once
#include "../GonioMeter/Goniometer.h"
using namespace juce;

//==============================================================================
struct CorrelationMeter : juce::Component
{
    // Override of the paint function to handle the drawing of the correlation meter
    void paint(juce::Graphics& g) override;

    // Function to update the correlation meter with the readings measured on the audio thread
    void update(float fastCorrelation, float slowCorrelation);

private:
    // The fast and slow correlation readings, from -1 to +1
    float fastCorrelation = 0.f, slowCorrelation = 0.f;

    // Function to draw the average on the correlation meter
    void drawAverage(juce::Graphics& g,
        juce::Rectangle<int> bounds,
        float avg,
        bool drawBorder);
};

