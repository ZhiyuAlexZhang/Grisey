#pragma once

#include <JuceHeader.h>
#include "../UI/Theme.h"
#include "../UI/CachedLayer.h"
#include "SpectrumSource.h"

//==============================================================================
// Draws the spectra that the SpectrumSource analyzes: two curves, their peak holds, and a strip
// along the bottom that shows the correlation of the channels in each band. Under the mouse it
// reads out the frequency, the nearest note, and the level of the first curve.
class SpectrumView : public juce::Component
{
public:
    static constexpr float maxDecibels = 6.f;
    static constexpr float minDecibels = -96.f;
    static constexpr double minFrequency = 20.0;
    static constexpr double maxFrequency = 20000.0;

    // The correlation of each point is taken over a band of at least this width
    static constexpr float correlationBandOctaves = 1.f / 3.f;

    // How the curves move. They rise to a new level almost at once, and fall back slowly and
    // smoothly, which is easier to read than a curve that follows every frame, and they move in
    // every frame, whether or not there is a new spectrum, so the motion is never jerky.
    static constexpr float attackSeconds = 0.025f;
    static constexpr float releaseSeconds = 0.30f;

    // Behind each curve are faint copies of where it was a moment ago, which show how it is moving
    static constexpr int numTrails = 3;
    static constexpr float trailIntervalSeconds = 0.07f;

    // A trail is faint, so it is drawn with a point for every few of the curve's, which costs less
    static constexpr int trailPointStep = 3;

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

    explicit SpectrumView(SpectrumSource&);

    void paint(juce::Graphics&) override;
    void resized() override;

    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

    // Clicking the view restarts the peak hold
    void mouseDown(const juce::MouseEvent&) override;

    // Restarts the peak hold from the current curves
    void resetPeakHold();

    // Moves the curves towards the source's spectra, called by the editor once per frame.
    // hasNewSpectra says whether the source has analyzed new audio since the last call. While
    // held, the curves stand still.
    void update(bool hasNewSpectra, const Settings& newSettings, float elapsedSeconds, bool held);

private:
    void paintGrid(juce::Graphics& g);
    void paintReadout(juce::Graphics& g);

    // Builds the path of a curve within the plot. A closed path runs along the bottom for filling.
    // pointStep is how many of the curve's points each point of the path stands for.
    juce::Path makePath(const std::vector<float>& decibels, bool closed, int pointStep = 1) const;

    float xOf(double frequency) const;
    float yOf(float decibels) const;
    double frequencyAt(float x) const;

    SpectrumSource& source;

    // The plot is the view less the margins that hold the labels of the axes
    juce::Rectangle<int> plot;

    // In decibels, one value per display point: where the curves are heading, which is the latest
    // spectrum, where they are drawn, which follows it, and the highest that they have been
    std::array<std::vector<float>, 2> curves, shown, peakHolds;

    // Where the curves were drawn a moment ago, the oldest first once they are in order
    std::array<std::array<std::vector<float>, 2>, numTrails> trails;
    int nextTrail = 0;
    float secondsSinceTrail = 0.f;
    std::vector<float> correlation;

    // One pixel per display point, which paint stretches over the strip
    juce::Image correlationStrip;

    // The settings that the curves were last laid out with
    SpectrumEngine::Display display;
    Settings settings;

    CachedLayer grid;

    // Where the mouse is, if it is over the plot
    std::optional<juce::Point<int>> hover;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumView)
};
