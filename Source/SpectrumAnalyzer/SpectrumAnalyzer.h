

#pragma once
#include <JuceHeader.h>
#include "../Constants.h"
#include "../PluginProcessor.h"

//==============================================================================
// Enumeration FFTOrder
// Represents different orders for Fast Fourier Transform (FFT)
enum FFTOrder
{
    order2048 = 11,
    order4096 = 12,
    order8192 = 13
};

//==============================================================================
// Class definition for LogarithmicScale
class LogarithmicScale : public juce::Component
{
public:
    // Constructor
    LogarithmicScale();
    // Destructor
    ~LogarithmicScale() override;

    // Overrides the paint function to draw the logarithmic scale
    void paint(juce::Graphics&) override;
    // Overrides the resized function to handle component resizing
    void resized() override;

    // Function to set the grid color
    void setGridColour(juce::Colour);
    // Function to set the text color
    void setTextColour(juce::Colour);

private:
    // Function to calculate the base ten logarithm for frequency
    void calculateBaseTenLogarithm();
    // Function to calculate the frequency grid
    void calculateFrequencyGrid();
    // Function to add labels to the scale
    void addLabels();

    // Function to calculate the offset in hertz
    int getOffsetInHertz(const int);
    // Function to get the current frequency in hertz
    int getCurrentFrequencyInHertz(const int, const int);

    // Color for the grid
    juce::Colour gridColor { 0xff464646 };
    // Color for the text
    juce::Colour textColor { 0xff848484 };

    // Coefficient for the logarithmic scale
    int coefficient{ 10 };
    // Maximum frequency in hertz
    int maxFreqHz{ 20000 };
    // Minimum frequency in hertz
    int minFreqHz{ 20 };

    // Map to store base ten logarithms
    std::map<int, float> baseTenLog;
    // Map to store frequency grid points
    std::map<int, float> freqGridPoints;
    // Map to store labels
    std::map<int, std::unique_ptr<juce::Label>> labels;

    // Macro to declare the class as non-copyable with leak detector
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LogarithmicScale)
};

//==============================================================================
// Class definition for SpectrumGrid
class SpectrumGrid :
    public juce::Component
{
public:
    // Constructor
    SpectrumGrid(juce::AudioProcessorValueTreeState&);
    // Destructor
    ~SpectrumGrid() override;

    // Overrides the paint function to draw the grid
    void paint(juce::Graphics&) override;
    // Overrides the resized function to handle component resizing
    void resized() override;

    // Function to set the grid color
    void setGridColour(juce::Colour);
    // Function to set the text color
    void setTextColour(juce::Colour);

private:
    // Function to set the volume range in decibels
    void setVolumeRangeInDecibels(const int, int);
    // Function to calculate the amplitude grid
    void calculateAmplitudeGrid();
    // Function to add labels to the grid
    void addLabels();

    // Reference to the audio processor's value tree state
    juce::AudioProcessorValueTreeState& mr_audioProcessorValueTreeState;
    // Logarithmic scale object
    LogarithmicScale m_logarithmicScale;
    // Color for the grid
    juce::Colour gridColor { 0xff464646 };
    // Color for the text
    juce::Colour textColor { 0xff848484 };
    // Atomic boolean to indicate if the grid style is logarithmic
    std::atomic<bool> m_gridStyleIsLogarithmic { true };
    // Atomic integers for maximum and minimum decibels, first offset, and offset decibel
    std::atomic<int> maxDecibel { 12 };
    std::atomic<int> minDecibel { -120 };
    std::atomic<int> firstOffset;
    std::atomic<int> offsetDecibel;
    // Vector to store grid points
    std::vector<float> gridPoints;
    // Map to store labels
    std::map<int, std::unique_ptr<juce::Label>> labels;

    // Macro to declare the class as non-copyable with leak detector
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumGrid)
};

//==============================================================================
// Struct definition for FFTDataGenerator
struct FFTDataGenerator
{
    // Function to produce FFT data suitable for rendering, from getFFTSize() samples
    const std::vector<float>& produceFFTDataForRendering(const float* audioData, const float negativeInfinity)
    {
        // Get the FFT size
        const auto fftSize = getFFTSize();

        // Reset the FFT data and copy audio data into it
        fftData.assign(fftData.size(), 0);
        std::copy(audioData, audioData + fftSize, fftData.begin());

        // Apply windowing to the FFT data
        window->multiplyWithWindowingTable(fftData.data(), (size_t)fftSize);

        // Perform forward FFT
        forwardFFT->performFrequencyOnlyForwardTransform(fftData.data());

        // Normalize FFT data and convert to decibels
        int numBins = (int)fftSize / 2;
        for (int i = 0; i < numBins; ++i)
        {
            auto v = fftData[(size_t)i];
            if (!std::isinf(v) && !std::isnan(v))
            {
                v /= float(numBins);
            }
            else
            {
                v = 0.f;
            }
            fftData[(size_t)i] = juce::Decibels::gainToDecibels(v, negativeInfinity);
        }

        return fftData;
    }

    // Function to change the FFT order
    void changeOrder(FFTOrder newOrder)
    {
        // Update the FFT order
        order = newOrder;
        auto fftSize = getFFTSize();

        // Recreate forward FFT and windowing objects
        forwardFFT = std::make_unique<juce::dsp::FFT>(order);
        window = std::make_unique<juce::dsp::WindowingFunction<float>>((size_t)fftSize, juce::dsp::WindowingFunction<float>::blackmanHarris);

        // Clear and resize the FFT data buffer
        fftData.clear();
        fftData.resize((size_t)fftSize * 2, 0);
    }

    // Function to get the FFT size
    int getFFTSize() const
    {
        return 1 << order;
    }

private:
    FFTOrder order = FFTOrder::order2048; // Order of the FFT
    std::vector<float> fftData; // Buffer for FFT data
    std::unique_ptr<juce::dsp::FFT> forwardFFT; // Forward FFT object
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window; // Windowing function object
};

//==============================================================================
// Struct definition for AnalyzerPathGenerator
struct AnalyzerPathGenerator
{
    // Function to generate a path based on render data, FFT bounds, etc.
    void generatePath(juce::Path& p,
        const std::vector<float>& renderData,
        juce::Rectangle<float> fftBounds,
        int fftSize,
        float binWidth,
        float negativeInfinity)
    {
        // Extract FFT bounds properties
        auto top = fftBounds.getY();
        auto bottom = fftBounds.getHeight();
        auto width = fftBounds.getWidth();

        // Calculate the number of FFT bins
        int numBins = (int)fftSize / 2;

        // Reuse the path's storage from the previous frame
        p.clear();
        p.preallocateSpace(3 * numBins);

        // Lambda function to map render data to y-coordinates
        auto map = [bottom, top, negativeInfinity](float v)
        {
            return juce::jmap(v, negativeInfinity, 0.f, float(bottom + 1), top);
        };

        // Map the first render data point to a y-coordinate
        auto y = map(renderData[0]);
        // Check for NaN or infinity
        jassert(!std::isnan(y) && !std::isinf(y));
        // Start a new subpath at (0, y)
        p.startNewSubPath(0, y);

        // Define the resolution for the path
        const int pathResolution = 1;

        // Iterate over the bins and create path segments
        for (int binNum = 1; binNum < numBins; binNum += pathResolution)
        {
            // Map the render data to a y-coordinate
            y = map(renderData[(size_t)binNum]);

            // If y-coordinate is not NaN or infinity, create a path segment
            if (!std::isnan(y) && !std::isinf(y))
            {
                // Calculate the frequency of the bin
                auto binFreq = (float)binNum * binWidth;
                // Normalize the bin's x-coordinate
                auto normalizedBinX = juce::mapFromLog10(binFreq, 20.f, 20000.f);
                // Calculate the actual x-coordinate in the FFT bounds
                auto binX = std::floor(normalizedBinX * width);
                // Add a line segment to the path
                p.lineTo(binX, y);
            }
        }
    }
};

//==============================================================================
// Struct definition for PathProducer
struct PathProducer
{
    // Constructor for PathProducer
    PathProducer()
    {
        // Initialize the FFT data generator and set the FFT order to 2048
        fftDataGenerator.changeOrder(FFTOrder::order2048);
    }

    // Function to get the number of samples that process() needs
    int getFFTSize() const { return fftDataGenerator.getFFTSize(); }

    // Function to produce the path from the most recent getFFTSize() samples of one channel
    void process(const float* samples, juce::Rectangle<float> fftBounds, double sampleRate);

    // Function to get the path
    const juce::Path& getPath() const { return fftPath; }

private:
    // FFT data generator for the channel
    FFTDataGenerator fftDataGenerator;
    // Path generator for analyzer
    AnalyzerPathGenerator pathGenerator;
    // Path for the FFT of the channel
    juce::Path fftPath;
};

//==============================================================================
// Class definition for ResponseCurveComponent
struct ResponseCurveComponent : juce::Component
{
    // Constructor
    ResponseCurveComponent(MultiMeterAudioProcessor&);

    // Overrides the paint function to draw the component
    void paint(juce::Graphics&) override;

    // Overrides the paintOverChildren function to draw on top of the children
    void paintOverChildren(Graphics& g) override;

    // Analyzes the most recent audio and repaints, called by the editor once per frame
    void update();

    // Overrides the resized function to handle resizing of the component
    void resized() override;

private:
    // Reference to the audio processor
    MultiMeterAudioProcessor& audioProcessor;

    // Colors for left and right channels
    juce::Colour leftChannelColour { 0xff48bde8 };
    juce::Colour rightChannelColour { 0xffa0a0a0 };

    // Grid for spectrum analysis
    SpectrumGrid logGrid;

    // Function to get the area to render
    juce::Rectangle<int> getRenderArea();

    // Function to get the area for analysis
    juce::Rectangle<int> getAnalysisArea();

    // Path producers for left and right channels
    PathProducer leftPathProducer, rightPathProducer;

    // The most recent samples of both channels, which the FFTs analyze
    juce::AudioBuffer<float> analysisBuffer;

    // The ring buffer's sample count at the last analysis, to skip frames without new audio
    juce::uint64 lastTotalWritten = 0;
};
