

#pragma once
#include <JuceHeader.h>
#include "../Constants.h"
#include "../PluginProcessor.h"
#include "SpectrumSource.h"

//==============================================================================
// Enumeration FFTOrder

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
// Class definition for SpectrumAnalyzer
// Draws the spectra that the SpectrumSource analyzes: two curves, their peak holds,
// and a strip along the bottom that shows the correlation of the channels in each band
struct SpectrumAnalyzer : juce::Component
{
    // The range of the grid in decibels, which the curves are drawn against
    static constexpr float maxDecibels = 12.f;
    static constexpr float minDecibels = -120.f;

    // The correlation of each point is taken over a band of at least this width
    static constexpr float correlationBandOctaves = 1.f / 3.f;

    // Bands that are quieter than this have no correlation to show. The correlation of a noise floor
    // is as random as the noise, so the strip only speaks where there is signal.
    static constexpr float correlationQuietDb = -72.f;

    // How the spectra are shown, which the editor reads from the parameters in every update
    struct Settings
    {
        bool midSide = false;          // Mid and side in place of left and right
        float tiltDbPerOctave = 0.f;
        float smoothingOctaves = 0.f;
        bool peakHold = false;

        bool operator==(const Settings& other) const
        {
            return midSide == other.midSide && peakHold == other.peakHold
                && juce::exactlyEqual(tiltDbPerOctave, other.tiltDbPerOctave)
                && juce::exactlyEqual(smoothingOctaves, other.smoothingOctaves);
        }
    };

    // Constructor
    SpectrumAnalyzer(juce::AudioProcessorValueTreeState&, SpectrumSource&);

    // Overrides the paint function to draw the component
    void paint(juce::Graphics&) override;

    // Overrides the paintOverChildren function to draw on top of the children
    void paintOverChildren(Graphics& g) override;

    // Overrides the resized function to handle resizing of the component
    void resized() override;

    // Clicking the analyzer restarts the peak hold
    void mouseDown(const juce::MouseEvent&) override;

    // Lays out the curves again from the source's spectra, called by the editor once per frame.
    // hasNewSpectra says whether the source has analyzed new audio since the last call.
    void update(bool hasNewSpectra, const Settings& newSettings);

private:
    // Builds the path of a curve within the analysis area. A closed path runs along the bottom for filling.
    juce::Path makePath(const std::vector<float>& decibels, juce::Rectangle<float> area, bool closed) const;

    // Function to get the area to render
    juce::Rectangle<int> getRenderArea();

    // Function to get the area for analysis
    juce::Rectangle<int> getAnalysisArea();

    // Where the spectra come from
    SpectrumSource& source;

    // Colors for the first curve (left or mid) and the second (right or side)
    juce::Colour firstCurveColour { 0xff48bde8 };
    juce::Colour secondCurveColour { 0xffa0a0a0 };

    // Colors for the correlation strip
    juce::Colour inPhaseColour { 0xff48bde8 };
    juce::Colour outOfPhaseColour { 0xffe85c48 };

    // Grid for spectrum analysis
    SpectrumGrid logGrid;

    // The curves in decibels, one value per display point
    std::array<std::vector<float>, 2> curves, peakHolds;
    std::vector<float> correlation;

    // One pixel per display point, which paint stretches over the strip
    juce::Image correlationStrip;

    // The settings that the curves were last laid out with
    SpectrumEngine::Display display;
    Settings settings;
};
