/*
  ==============================================================================

    The editor of the plugin: a header with a tab for each view, the view itself,
    a column of meters that is always showing, and a bar of controls for the view.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/Theme.h"
#include "UI/LookAndFeel.h"
#include "UI/Controls.h"
#include "UI/ControlBar.h"
#include "Views/SpectrumSource.h"
#include "Views/GoniometerView.h"
#include "Views/SpectrumView.h"
#include "Views/SpectrogramView.h"
#include "Views/HistoryView.h"
#include "Views/LoudnessView.h"
#include "Views/LevelMeters.h"
#include "Views/LoudnessSummary.h"
#include "Views/Timeline.h"

//==============================================================================
class GriseyAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    GriseyAudioProcessorEditor (GriseyAudioProcessor&);
    ~GriseyAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    static constexpr int defaultWidth = 1000, defaultHeight = 580;
    static constexpr int minWidth = 860, minHeight = 480;
    static constexpr int maxWidth = 2600, maxHeight = 1600;

    // The readings count as silence when no audio has arrived for this long, which is
    // what happens when the host stops calling the processor
    static constexpr double silenceTimeoutSeconds = 0.25;

    // Called before every frame that the display presents, with the time of that frame in seconds.
    // It updates the meters at the refresh rate that the user has chosen, and skips the frames in
    // between. Updates that are in step with the display were measured to cost less than a timer at
    // the same rate. The meters move by the time that has passed, so they behave the same at any rate.
    void vBlank(double timestampSeconds);

    // Reads the measurements made on the audio thread and updates every meter
    void updateMeters(float elapsedSeconds);

    // Shows the view that the main view parameter selects
    void showMainView(int viewId);

    // Draws the header and the bottom bar, whose raised parts meet the recessed ones in S-shaped shoulders
    void paintHeader(juce::Graphics& g, juce::Rectangle<int> area);
    void paintBottomBar(juce::Graphics& g, juce::Rectangle<int> area);

    // Fills the menu of the settings that the meters of the side column share
    void buildMeterSettingsMenu(juce::PopupMenu& menu);

    // The current value of a parameter: as it is, as the index of a choice, or as a switch
    float getValue(const juce::String& parameterID) const;
    int getChoice(const juce::String& parameterID) const;
    bool isOn(const juce::String& parameterID) const;

    // This reference is provided as a quick way for your editor to access the processor object that created it
    GriseyAudioProcessor& audioProcessor;

    GriseyLookAndFeel lookAndFeel;
    TabBar tabs;

    // The views, of which one is showing. The spectrum and the spectrogram draw the same spectra.
    SpectrumSource spectrumSource;
    GoniometerView goniometerView;
    SpectrumView spectrumView;
    SpectrogramView spectrogramView;
    HistoryView historyView;
    LoudnessView loudnessView;

    // The side column, which is always showing
    LevelMeters levelMeters;
    LoudnessSummary loudnessSummary;
    CorrelationBar correlationBar;

    // The bottom bar
    ControlBar controlBar;
    SettingsButton meterSettingsButton { "Meters" };
    juce::Button* freezeButton = nullptr;

    juce::ParameterAttachment mainViewAttachment;

    // Whether the constructor has finished, before which a change of size is not the user's
    bool isConstructed = false;

    // Set by the reset items of the menus, and cleared by the next update
    bool resetTicksRequested = false;

    // The clock of the timeline that the spectrogram, the history and the loudness share
    Timeline::Clock timelineClock;

    // Timing of the updates, in seconds
    double lastUpdateTime = -1.0;
    double lastAudioTime = 0.0;

    // The numbers take new readings ten times a second, and all at the same moment, so that a
    // reading that is shown in two places says the same in both
    double secondsSinceReadout = 0.0;
    juce::uint64 lastTotalWritten = 0;

    // Declared last, so that the callbacks stop before anything that they use is destroyed
    juce::VBlankAttachment vBlankAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GriseyAudioProcessorEditor)
};
