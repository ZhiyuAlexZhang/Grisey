

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
// Implementation for the PathProducer class
void PathProducer::process(const float* samples, juce::Rectangle<float> fftBounds, double sampleRate)
{
    const auto fftSize = fftDataGenerator.getFFTSize();
    const auto binWidth = sampleRate / double(fftSize);

    // Produce FFT data for rendering, and generate the path from it
    const auto& fftData = fftDataGenerator.produceFFTDataForRendering(samples, -120.f);
    pathGenerator.generatePath(fftPath, fftData, fftBounds, fftSize, (float)binWidth, -120.f);
}

//==============================================================================
// Implementation for the ResponseCurveComponent class
// Constructor for ResponseCurveComponent
ResponseCurveComponent::ResponseCurveComponent(MultiMeterAudioProcessor& p) : audioProcessor(p),
logGrid(p.apvts)
{
    // Both channels are analyzed with the same FFT size
    analysisBuffer.setSize(2, leftPathProducer.getFFTSize());
    analysisBuffer.clear();

    // Add the logGrid component and make it visible
    addAndMakeVisible(logGrid);
    // Set the color of the grid
    logGrid.setGridColour(juce::Colour(0xff464646));
    // Set the text color of the grid
    logGrid.setTextColour(juce::Colour(0xff848484));
}

// Paint function for ResponseCurveComponent
void ResponseCurveComponent::paint(juce::Graphics& g)
{
    // Fill a rounded rectangle with the background color
    g.setColour(BASE_COLOR);
    g.fillRect(getAnalysisArea());
}

// Function to paint over the children of ResponseCurveComponent
void ResponseCurveComponent::paintOverChildren(Graphics& g)
{
    // Get the area for response analysis
    auto responseArea = getAnalysisArea();

    // Get the path for FFT of the right channel
    auto rightChannelFFTPath = rightPathProducer.getPath();
    // Translate the path to response area
    rightChannelFFTPath.applyTransform(AffineTransform().translation(responseArea.getX(), responseArea.getY()));
    // Set the color and stroke the path
    g.setColour(rightChannelColour);
    g.strokePath(rightChannelFFTPath, PathStrokeType(1.f));

    // Get the path for FFT of the left channel
    auto leftChannelFFTPath = leftPathProducer.getPath();
    // Translate the path to response area
    leftChannelFFTPath.applyTransform(AffineTransform().translation(responseArea.getX(), responseArea.getY()));
    // Set the color and stroke the path
    g.setColour(leftChannelColour);
    g.strokePath(leftChannelFFTPath, PathStrokeType(1.f));

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

// Update function for ResponseCurveComponent
void ResponseCurveComponent::update()
{
    auto& ringBuffer = audioProcessor.sampleRingBuffer;

    // There is nothing new to draw if no audio has arrived since the last frame
    const auto totalWritten = ringBuffer.getTotalWritten();
    if (totalWritten == lastTotalWritten)
        return;

    // Keep the previous curves if the audio thread overwrote the samples during the copy
    if (!ringBuffer.readLatest(analysisBuffer.getWritePointer(0), analysisBuffer.getWritePointer(1), analysisBuffer.getNumSamples()))
        return;

    lastTotalWritten = totalWritten;

    // Get the bounds for FFT analysis
    auto fftBounds = getAnalysisArea().toFloat();
    // Get the sample rate
    auto sampleRate = audioProcessor.getSampleRate();

    // Process FFT for left and right channels
    leftPathProducer.process(analysisBuffer.getReadPointer(0), fftBounds, sampleRate);
    rightPathProducer.process(analysisBuffer.getReadPointer(1), fftBounds, sampleRate);
    // Repaint the component
    repaint();
}

// Resized function for ResponseCurveComponent
void ResponseCurveComponent::resized()
{
    // Set the bounds for logGrid
    logGrid.setBounds(getAnalysisArea());
}

// Function to get the render area
juce::Rectangle<int> ResponseCurveComponent::getRenderArea()
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop(7);
    bounds.removeFromBottom(7);
    bounds.removeFromLeft(20);
    bounds.removeFromRight(20);
    return bounds;
}

// Function to get the analysis area
juce::Rectangle<int> ResponseCurveComponent::getAnalysisArea()
{
    auto bounds = getRenderArea();
    bounds.removeFromTop(4);
    bounds.removeFromBottom(4);
    return bounds;
}
