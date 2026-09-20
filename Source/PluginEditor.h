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
#include "LevelMeter/LevelMeter.h"
#include "CorrelationMeter/CorrelationMeter.h"
#include "Controls/Buttons.h"
#include "Controls/Slider.h"
 
//==============================================================================
class MultiMeterAudioProcessorEditor  : public juce::AudioProcessorEditor, juce::Timer, juce::ComboBox::Listener, juce::ToggleButton::Listener, juce::Slider::Listener
{
public:
    MultiMeterAudioProcessorEditor (MultiMeterAudioProcessor&);
    ~MultiMeterAudioProcessorEditor() override;
    
    void paint(juce::Graphics&) override;

    void resized() override;

    void timerCallback() override;

    void comboBoxChanged(juce::ComboBox* comboBox) override;
    void buttonClicked(juce::Button* button) override;
    void sliderValueChanged(juce::Slider* slider) override;
    
    juce::AudioBuffer<float> buffer{2, 256};
    StereoMeter peakMeter{"PEAK"}, RMSMeter{"RMS"};
    Histogram peakHistogram{"PEAK"}, rmsHistogram{"RMS"};
    
private:
    // This reference is provided as a quick way for your editor to access the processor object that created it
    MultiMeterAudioProcessor& audioProcessor;
    Goniometer gonioMeter;
    CorrelationMeter correlationMeter;
    ResponseCurveComponent spectrumAnalyzer;

    ButtonsLook lookAndFeel;
    SwitchButton menuViewSwitch;

    // All combobox controls are defined here
    juce::ComboBox levelMeterDecaySelector, averagerDurationSelector, holdTimeSelector;
    Switch tickDisplay{ "Hide Tick","Show Tick" }, resetHold{"Reset Hold","Reset Hold"};

    ToggleChain histogramViewButton, meterViewButton;

    juce::Label levelMeterDecayLabel, averagerDurationLabel, meterViewLabel, holdTimeLabel, histogramViewLabel, tickDisplayLabel, correlationLabel0, correlationLabel1, correlationLabel2, scaleKnobLabel;

    float currentDecayRate = 3.f; // Define a variable to hold the current decay rate being used and update the variable from combo box changed callback function
    float holdTime = 2.f; // Use this variable to store hold time
    juce::int64 averagerDuration = 100; // Use the default average time 100 for averager

    // Define bounds for side by side and stacked histogram positions
    juce::Rectangle<int> peakSBS, rmsSBS, peakStacked, rmsStacked;

    RotarySliderWithLabels scaleKnobSlider;
    
    // Only goniometer scale value is used with apvts and other controls are used freely
    using APVTS = juce::AudioProcessorValueTreeState;
    using Attachment = APVTS::SliderAttachment;  
    Attachment scaleKnobSliderAttachment;
 
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultiMeterAudioProcessorEditor)
};
