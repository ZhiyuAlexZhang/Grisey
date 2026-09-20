

#include "SpectrumAnalyzer.h"

//==============================================================================
// Implementation for the LogarithmicScale class
// Constructor for LogarithmicScale
LogarithmicScale::LogarithmicScale()
{
    // Calculate base ten logarithm and add labels
    calculateBaseTenLogarithm();
    addLabels();
}

// Destructor for LogarithmicScale
LogarithmicScale::~LogarithmicScale() {}

// Overrides the paint function to draw the logarithmic scale
void LogarithmicScale::paint(juce::Graphics& g)
{
    // Set the grid color
    g.setColour(gridColor);

    // Draw vertical lines for frequency grid
    for (const auto& [frequency, x] : freqGridPoints)
    {
        g.drawLine(x, 0, x, getHeight());
    }

    // Position labels on the frequency grid
    for (const auto& [frequency, label] : labels)
    {
        label->setBounds(freqGridPoints[frequency] - 14, 1, 28, 20);
    }
}

void LogarithmicScale::resized()
{
    calculateFrequencyGrid();
}

void LogarithmicScale::setGridColour(juce::Colour colour)
{
    gridColor = colour;
}

void LogarithmicScale::setTextColour(juce::Colour colour)
{
    textColor = colour;
}

// Function to calculate the base ten logarithm for the frequency range
void LogarithmicScale::calculateBaseTenLogarithm()
{
    // Calculate the offset in hertz based on the minimum frequency
    auto offsetInHertz = getOffsetInHertz(minFreqHz);

    // Get the current frequency in hertz based on the minimum frequency and offset
    auto currentFrequencyInHertz = getCurrentFrequencyInHertz(minFreqHz, offsetInHertz);

    // If the current frequency is not equal to the minimum frequency, calculate its base ten logarithm
    if (currentFrequencyInHertz != minFreqHz)
    {
        baseTenLog[minFreqHz] = std::log10f(static_cast<float>(minFreqHz));
    }

    // Loop until the current frequency is less than the maximum frequency
    while (currentFrequencyInHertz < maxFreqHz)
    {
        // Calculate the base ten logarithm for the current frequency
        baseTenLog[currentFrequencyInHertz] = std::log10f(static_cast<float>(currentFrequencyInHertz));

        // If the current frequency matches the multiplication of offset and coefficient, update the offset
        if (offsetInHertz * coefficient == currentFrequencyInHertz)
        {
            offsetInHertz *= coefficient;
        }

        // Increment the current frequency by the offset
        currentFrequencyInHertz += offsetInHertz;
    }

    // If the maximum frequency is reached or surpassed, calculate its base ten logarithm
    if (maxFreqHz <= currentFrequencyInHertz)
    {
        baseTenLog[maxFreqHz] = std::log10f(static_cast<float>(maxFreqHz));
    }
}

// Function to calculate frequency grid points based on the logarithmic scale
void LogarithmicScale::calculateFrequencyGrid()
{
    // Get the minimum and maximum values of base ten logarithm from the frequency range
    auto sourceRangeMinimum = (baseTenLog.begin())->second;
    auto sourceRangeMaximum = (--baseTenLog.end())->second;

    // Define the target range for the frequency grid points
    auto targetRangeMinimum = 0.0f;
    auto targetRangeMaximum = static_cast<float>(getWidth()); // Assuming getWidth() returns the width of the component

    // Clear the map storing frequency grid points
    freqGridPoints.clear();

    // Iterate over each frequency and its corresponding base ten logarithm
    for (const auto& [frequency, value] : baseTenLog)
    {
        // Map the base ten logarithm value to the target range using juce::jmap
        freqGridPoints[frequency] = juce::jmap(value, sourceRangeMinimum, sourceRangeMaximum, targetRangeMinimum, targetRangeMaximum);
    }
}

// Function to add labels to the frequency grid
void LogarithmicScale::addLabels()
{
    // Iterate over frequencies from 100 Hz to 10 kHz with a factor of 10
    for (auto frequency = 100; frequency <= 10000; frequency *= 10)
    {
        // Insert a new label for each frequency into the map 'labels'
        labels.insert(std::pair<int, std::unique_ptr<juce::Label>>(frequency, new juce::Label()));
    }

    // Iterate over each label in the 'labels' map
    for (const auto& [frequency, label] : labels) {
        // Make the label visible
        addAndMakeVisible(*label);

        // Set text for the label depending on the frequency
        label->setText(
            // If frequency is 100 Hz, set text as '100', otherwise set text as 'xk' (where x is frequency in kHz)
            frequency == 100 ? juce::String(frequency) : juce::String(frequency / 1000) + "k",
            juce::NotificationType::dontSendNotification
        );

        // Set font size for the label
        label->setFont(12);

        // Set text color for the label
        label->setColour(juce::Label::textColourId, textColor);

        // Set justification type for the label
        label->setJustificationType(juce::Justification::centredTop);
    }
}

// Function to calculate the offset in Hertz based on the frequency
int LogarithmicScale::getOffsetInHertz(const int frequency)
{
    // Initialize variables for calculating the offset
    auto minimumForDivisions = frequency;
    auto divisionCounter = 1;

    // Calculate the offset using logarithmic division until the division exceeds the coefficient
    while (coefficient < (minimumForDivisions / coefficient))
    {
        minimumForDivisions /= coefficient;
        ++divisionCounter;
    }

    // Return the calculated offset
    return static_cast<int>(pow(static_cast<float>(coefficient), divisionCounter));
}

// Function to get the current frequency in Hertz based on the offset
int LogarithmicScale::getCurrentFrequencyInHertz(const int currentFrequencyInHertz, const int offsetInHertz)
{
    // If the current frequency is divisible by the offset, return the current frequency
    if (currentFrequencyInHertz % offsetInHertz == 0)
    {
        return currentFrequencyInHertz;
    }
    // Otherwise, calculate and return the new frequency adjusted to the nearest multiple of the offset
    else
    {
        auto newFrequency = currentFrequencyInHertz;
        newFrequency -= currentFrequencyInHertz % offsetInHertz;
        return newFrequency + offsetInHertz;
    }
}

//==============================================================================
// Implementation for the SpectrumGrid class
// Constructor for SpectrumGrid
SpectrumGrid::SpectrumGrid(juce::AudioProcessorValueTreeState& audioProcessorValueTreeState) :
    mr_audioProcessorValueTreeState(audioProcessorValueTreeState)
{
    // Add the logarithmic scale component as a child component
    addChildComponent(m_logarithmicScale);

}

// Destructor for SpectrumGrid
SpectrumGrid::~SpectrumGrid() {}

// Overrides the paint function to draw the grid
void SpectrumGrid::paint(juce::Graphics& g)
{
    // Set the grid color
    g.setColour(gridColor);
    // Draw the grid rectangle
    g.drawRect(getLocalBounds());

    // Calculate the amplitude grid
    calculateAmplitudeGrid();
    // Add labels to the grid
    addLabels();

    // Draw horizontal lines for the grid
    for (const auto y : gridPoints)
    {
        g.drawLine(0.0f, y, static_cast<float>(getWidth()), y);
    }

    // Position labels on the grid
    for (const auto& [volume, label] : labels)
    {
        label->setBounds(0.0f,
            juce::jmap(static_cast<float>(volume),
                static_cast<float>(maxDecibel.load()),
                static_cast<float>(minDecibel.load()),
                0.0f,
                static_cast<float>(getHeight())) - 7.0f,
            28.0f,
            20.0f);
    }

    // Set visibility of the logarithmic scale component based on m_gridStyleIsLogarithmic
    m_logarithmicScale.setVisible(m_gridStyleIsLogarithmic.load());

}

// Overrides the resized function to handle component resizing
void SpectrumGrid::resized()
{
    // Set bounds for the logarithmic scale component
    m_logarithmicScale.setBounds(getLocalBounds());
    // Repaint the component
    repaint();
}

// Function to set the grid color
void SpectrumGrid::setGridColour(juce::Colour colour)
{
    gridColor = colour;
}

// Function to set the text color
void SpectrumGrid::setTextColour(juce::Colour colour)
{
    textColor = colour;
}

// Function to set the volume range in decibels
void SpectrumGrid::setVolumeRangeInDecibels(const int maximum, int minimum)
{
    // Ensure the range between maximum and minimum is at least 10 decibels
    if (maximum - 10 < minimum) { minimum = maximum - 10; }

    // Update atomic variables for maximum and minimum decibels
    maxDecibel.store(maximum);
    minDecibel.store(minimum);
}

// Function to calculate the amplitude grid
void SpectrumGrid::calculateAmplitudeGrid()
{
    // Load the maximum and minimum decibel values
    const auto maximum{ maxDecibel.load() };
    const auto minimum{ minDecibel.load() };

    // Calculate the range in decibels
    int rangeInDecibels;
    if (maximum < 0)
    {
        // If maximum decibel is negative, calculate range using absolute values
        rangeInDecibels = (minimum - maximum) * -1;
    }
    else if (0 <= minimum)
    {
        // If both maximum and minimum are non-negative, calculate range directly
        rangeInDecibels = maximum - minimum;
    }
    else
    {
        // If minimum decibel is negative, calculate range using absolute values
        rangeInDecibels = maximum + minimum * -1;
    }

    // Initialize offset decibel to 0 and offset to 0.0f
    offsetDecibel.store(0);
    auto offset{ 0.0f };

    // Determine the offset decibel increment until it reaches 16.0f
    while (offset < 16.0f)
    {
        // Increment offset decibel by 6
        offsetDecibel.store(offsetDecibel.load() + 6);

        // Calculate corresponding offset based on range and component height
        offset = juce::jmap(static_cast<float>(offsetDecibel.load()),
            0.0f,
            static_cast<float>(rangeInDecibels),
            0.0f,
            static_cast<float>(getHeight()));
    }

    // Set the first offset to the maximum decibel
    firstOffset.store(maximum);

    // Adjust the first offset to be a multiple of the offset decibel
    while (firstOffset.load() % offsetDecibel.load() != 0)
    {
        firstOffset.store(firstOffset.load() - 1);
    }

    // Calculate the y-coordinate of the first grid line
    const auto first{ juce::jmap(static_cast<float>(firstOffset.load()),
                                  static_cast<float>(maximum),
                                  static_cast<float>(minimum),
                                  0.0f,
                                  static_cast<float>(getHeight())) };

    // Calculate the height of the component
    const auto height{ static_cast<float>(getHeight()) };
    // Clear existing grid points
    gridPoints.clear();

    // Generate grid points with an increment of offset
    for (auto position{ first }; position < height; position += offset)
    {
        gridPoints.push_back(position);
    }
}

// Function to add labels to the grid
void SpectrumGrid::addLabels()
{
    // Get the initial volume, offset, and minimum decibel values
    auto volume{ firstOffset.load() };
    const auto offset{ offsetDecibel.load() };
    const auto minimum{ minDecibel.load() };

    // Clear existing labels
    labels.clear();

    // Add labels to the grid based on the volume range
    while (minimum < volume - offset)
    {
        // Decrement the volume by the offset
        volume -= offset;
        // Create a new label and insert it into the map
        labels.insert(
            std::pair<int, std::unique_ptr<juce::Label>>(
                volume,
                new juce::Label()
            )
        );
    }

    // Set properties for each label and make them visible
    for (const auto& [volume, label] : labels) {
        // Add label to the component and make it visible
        addAndMakeVisible(*label);
        // Set text for the label
        label->setText(
            juce::String(volume),
            juce::NotificationType::dontSendNotification
        );
        // Set font size for the label
        label->setFont(12);
        // Set text color for the label
        label->setColour(juce::Label::textColourId, textColor);
        // Set text justification for the label
        label->setJustificationType(juce::Justification::centredTop);
    }
}

//==============================================================================
// Implementation for the SpectrumAnalyzer class
// Constructor for SpectrumAnalyzer
SpectrumAnalyzer::SpectrumAnalyzer(juce::AudioProcessorValueTreeState& apvts, SpectrumSource& spectrumSource) :
source(spectrumSource),
logGrid(apvts)
{
    // Add the logGrid component and make it visible
    addAndMakeVisible(logGrid);
    // Set the color of the grid
    logGrid.setGridColour(juce::Colour(0xff464646));
    // Set the text color of the grid
    logGrid.setTextColour(juce::Colour(0xff848484));
    // The clicks that restart the peak hold have to reach this component
    logGrid.setInterceptsMouseClicks(false, false);
}

// Paint function for SpectrumAnalyzer
void SpectrumAnalyzer::paint(juce::Graphics& g)
{
    // Fill a rounded rectangle with the background color
    g.setColour(BASE_COLOR);
    g.fillRect(getAnalysisArea());
}

// Function to paint over the children of SpectrumAnalyzer
void SpectrumAnalyzer::paintOverChildren(Graphics& g)
{
    // Get the area for response analysis
    auto responseArea = getAnalysisArea().toFloat();

    if (!curves[0].empty())
    {
        juce::Graphics::ScopedSaveState state(g);
        g.reduceClipRegion(getAnalysisArea());

        // The second curve (right or side) is drawn first, so that the first lies on top of it
        const std::array<juce::Colour, 2> colours { firstCurveColour, secondCurveColour };
        for (int index = 1; index >= 0; --index)
        {
            const auto i = (size_t)index;

            // Fill the curve with a gradient that fades out towards the bottom
            g.setGradientFill(juce::ColourGradient(colours[i].withAlpha(index == 0 ? 0.35f : 0.15f), 0.f, responseArea.getY(),
                colours[i].withAlpha(0.02f), 0.f, responseArea.getBottom(), false));
            g.fillPath(makePath(curves[i], responseArea, true));

            // Set the color and stroke the path
            g.setColour(colours[i]);
            g.strokePath(makePath(curves[i], responseArea, false), PathStrokeType(1.2f));

            if (showsPeakHold && peakHolds[i].size() == curves[i].size())
            {
                g.setColour(colours[i].withAlpha(0.55f));
                g.strokePath(makePath(peakHolds[i], responseArea, false), PathStrokeType(1.f));
            }
        }

        // The correlation strip runs along the bottom of the analysis area
        if (correlationStrip.isValid())
        {
            auto strip = responseArea.removeFromBottom(5.f);
            g.setColour(BASE_COLOR);
            g.fillRect(strip);
            g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
            g.drawImage(correlationStrip, strip, juce::RectanglePlacement::stretchToFit);
        }

        // Name the curves in their colors, below the frequency labels of the grid
        auto legend = getAnalysisArea().withTrimmedTop(22).removeFromTop(16).removeFromRight(44).translated(-4, 0);
        g.setFont(12.f);
        g.setColour(firstCurveColour);
        g.drawText(showsMidSide ? "M" : "L", legend.removeFromLeft(22), juce::Justification::centred);
        g.setColour(secondCurveColour);
        g.drawText(showsMidSide ? "S" : "R", legend, juce::Justification::centred);
    }

    // Create a border path
    Path border;
    border.setUsingNonZeroWinding(false);
    border.addRectangle(getAnalysisArea());
    auto bounds = getLocalBounds().toFloat();
    bounds.removeFromLeft(6);
    bounds.removeFromRight(6);
    border.addRoundedRectangle(bounds, 9);
    // Fill the border path
    g.setColour(BASE_COLOR);
    g.fillPath(border);
}

// Update function for SpectrumAnalyzer
void SpectrumAnalyzer::update(bool hasNewSpectra, bool midSide, float tiltDbPerOctave, float smoothingOctaves, bool peakHold)
{
    // Two points per pixel keep the curves smooth
    const int numPoints = juce::jmax(2, getAnalysisArea().getWidth() * 2);

    const bool settingsChanged = numPoints != display.numPoints
        || midSide != showsMidSide
        || !juce::exactlyEqual(tiltDbPerOctave, display.tiltDbPerOctave)
        || !juce::exactlyEqual(smoothingOctaves, display.smoothingOctaves);

    // The peak hold starts afresh whenever it would no longer be comparable
    if (settingsChanged || peakHold != showsPeakHold)
        for (auto& hold : peakHolds)
            hold.clear();

    if (!hasNewSpectra && !settingsChanged && peakHold == showsPeakHold)
        return;

    display.numPoints = numPoints;
    display.tiltDbPerOctave = tiltDbPerOctave;
    display.smoothingOctaves = smoothingOctaves;
    showsMidSide = midSide;
    showsPeakHold = peakHold;

    auto& engine = source.getEngine();
    const auto sampleRate = source.getSampleRate();

    engine.render(midSide ? SpectrumEngine::Curve::mid : SpectrumEngine::Curve::left, display, sampleRate, curves[0]);
    engine.render(midSide ? SpectrumEngine::Curve::side : SpectrumEngine::Curve::right, display, sampleRate, curves[1]);

    if (peakHold)
    {
        for (size_t i = 0; i < curves.size(); ++i)
        {
            if (peakHolds[i].size() != curves[i].size())
                peakHolds[i] = curves[i];

            for (size_t point = 0; point < curves[i].size(); ++point)
                peakHolds[i][point] = juce::jmax(peakHolds[i][point], curves[i][point]);
        }
    }

    // The correlation strip has one pixel per display point: blue where the channels are in phase,
    // red where they are out of phase, and clear where they are unrelated or silent
    engine.renderCorrelation(display, sampleRate, correlationBandOctaves, correlationQuietDb, correlation);

    if (correlationStrip.getWidth() != numPoints)
        correlationStrip = juce::Image(juce::Image::ARGB, numPoints, 1, true);

    {
        juce::Image::BitmapData pixels(correlationStrip, juce::Image::BitmapData::writeOnly);
        for (int point = 0; point < numPoints; ++point)
        {
            const float value = correlation[(size_t)point];
            const auto colour = std::isnan(value) ? juce::Colours::transparentBlack
                : (value >= 0.f ? inPhaseColour : outOfPhaseColour).withAlpha(std::abs(value));
            pixels.setPixelColour(point, 0, colour);
        }
    }

    // Repaint the component
    repaint();
}

juce::Path SpectrumAnalyzer::makePath(const std::vector<float>& decibels, juce::Rectangle<float> area, bool closed) const
{
    juce::Path path;
    if (decibels.size() < 2)
        return path;

    path.preallocateSpace(3 * (int)decibels.size() + 12);

    const float xStep = area.getWidth() / (float)(decibels.size() - 1);
    for (size_t point = 0; point < decibels.size(); ++point)
    {
        // A little below the bottom, so that the stroke of a silent curve is out of sight
        const float y = juce::jmap(juce::jmax(decibels[point], minDecibels - 3.f), minDecibels, maxDecibels, area.getBottom(), area.getY());
        const float x = area.getX() + xStep * (float)point;

        if (point == 0)
            path.startNewSubPath(x, y);
        else
            path.lineTo(x, y);
    }

    if (closed)
    {
        path.lineTo(area.getRight(), area.getBottom() + 4.f);
        path.lineTo(area.getX(), area.getBottom() + 4.f);
        path.closeSubPath();
    }

    return path;
}

void SpectrumAnalyzer::mouseDown(const juce::MouseEvent&)
{
    // Restart the peak hold from the current curves
    for (auto& hold : peakHolds)
        hold.clear();
}

// Resized function for SpectrumAnalyzer
void SpectrumAnalyzer::resized()
{
    // Set the bounds for logGrid
    logGrid.setBounds(getAnalysisArea());
}

// Function to get the render area
juce::Rectangle<int> SpectrumAnalyzer::getRenderArea()
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop(7);
    bounds.removeFromBottom(7);
    bounds.removeFromLeft(20);
    bounds.removeFromRight(20);
    return bounds;
}

// Function to get the analysis area
juce::Rectangle<int> SpectrumAnalyzer::getAnalysisArea()
{
    auto bounds = getRenderArea();
    bounds.removeFromTop(4);
    bounds.removeFromBottom(4);
    return bounds;
}
