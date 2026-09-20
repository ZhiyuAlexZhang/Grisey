/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.
    This project is built with JUCE version 9.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Histogram/Histogram.h"
#include "GonioMeter/Goniometer.h"
#include "SpectrumAnalyzer/SpectrumAnalyzer.h"
#include "Spectrogram/Spectrogram.h"
#include "LoudnessView/LoudnessView.h"
#include "LevelMeter/LevelMeter.h"
#include "CorrelationMeter/CorrelationMeter.h"
#include "Controls/Buttons.h"
#include "Controls/OptionsRow.h"
#include "Controls/Slider.h"

//==============================================================================
class MultiMeterAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    MultiMeterAudioProcessorEditor (MultiMeterAudioProcessor&);
    ~MultiMeterAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;

    void resized() override;

    StereoMeter peakMeter{"PEAK"}, RMSMeter{"RMS"};
    Histogram peakHistogram{"PEAK"}, rmsHistogram{"RMS"};

private:
    // The meters are updated at this rate whatever the refresh rate of the display is,
    // so that the histograms scroll and the averages settle at the same speed everywhere
    static constexpr double updateRateHz = 60.0;

    // The readings count as silence when no audio has arrived for this long, which is
    // what happens when the host stops calling the processor
    static constexpr double silenceTimeoutSeconds = 0.25;

    // Called before every frame that the display presents, with the time of that frame in seconds
    void vBlank(double timestampSeconds);

    // Reads the measurements made on the audio thread and updates every meter
    void updateMeters(float elapsedSeconds);

    // Shows the view that the main view parameter selects
    void showMainView(int viewId);

    // Lays out the histograms according to the histogram view parameter
    void layoutHistograms(int histogramViewId);

    // This reference is provided as a quick way for your editor to access the processor object that created it
    MultiMeterAudioProcessor& audioProcessor;
    Goniometer gonioMeter;
    CorrelationMeter correlationMeter;

    // The analyzer and the spectrogram draw the same spectra
    SpectrumSource spectrumSource;
    SpectrumAnalyzer spectrumAnalyzer;
    Spectrogram spectrogram;
    LoudnessView loudnessView;

    // The controls of the view that is showing
    OptionsRow optionsRow;
    juce::Button* freezeButton = nullptr;

    ButtonsLook lookAndFeel;
    SwitchButton menuViewSwitch;

    // All combobox controls are defined here
    juce::ComboBox levelMeterDecaySelector, averagerDurationSelector, holdTimeSelector;
    Switch tickDisplay{ "Hide Tick","Show Tick" }, resetHold{"Reset Hold","Reset Hold"};

    ToggleChain histogramViewButton, meterViewButton;

    juce::Label levelMeterDecayLabel, averagerDurationLabel, meterViewLabel, holdTimeLabel, histogramViewLabel, tickDisplayLabel, correlationLabel0, correlationLabel1, correlationLabel2, scaleKnobLabel;

    // Define bounds for side by side and stacked histogram positions
    juce::Rectangle<int> peakSBS, rmsSBS, peakStacked, rmsStacked;

    RotarySliderWithLabels scaleKnobSlider;

    // Every control is attached to its parameter, so the parameters hold all the settings
    using APVTS = juce::AudioProcessorValueTreeState;
    APVTS::SliderAttachment scaleKnobSliderAttachment;
    // The combo box attachments are created once the boxes have their items
    std::unique_ptr<APVTS::ComboBoxAttachment> levelMeterDecayAttachment, averagerDurationAttachment, holdTimeAttachment;
    APVTS::ButtonAttachment tickDisplayAttachment;
    juce::ParameterAttachment mainViewAttachment, meterViewAttachment, histogramViewAttachment;

    // The parameter values that the meters read in every update
    std::atomic<float>* scaleParameter = nullptr;
    std::atomic<float>* decayRateParameter = nullptr;
    std::atomic<float>* holdTimeParameter = nullptr;
    std::atomic<float>* meterViewParameter = nullptr;
    std::atomic<float>* showTickParameter = nullptr;
    std::atomic<float>* goniometerModeParameter = nullptr;
    std::atomic<float>* goniometerPersistenceParameter = nullptr;
    std::atomic<float>* spectrumChannelsParameter = nullptr;
    std::atomic<float>* spectrumTiltParameter = nullptr;
    std::atomic<float>* spectrumSmoothingParameter = nullptr;
    std::atomic<float>* spectrumResolutionParameter = nullptr;
    std::atomic<float>* spectrumPeakHoldParameter = nullptr;
    std::atomic<float>* loudnessTargetParameter = nullptr;

    // Set by the reset hold button, and cleared by the next update
    bool resetHoldRequested = false;

    // Timing of the updates
    double lastUpdateTime = -1.0;
    double lastAudioTime = 0.0;
    juce::uint64 lastTotalWritten = 0;

    // Declared last, so that the callbacks stop before anything that they use is destroyed
    juce::VBlankAttachment vBlankAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultiMeterAudioProcessorEditor)
};
