/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.
    This project is built with JUCE version 9.

  ==============================================================================
*/

#pragma once

// Macro used for testing.
#define USE_OSC false

#include <JuceHeader.h>
#include "Parameters.h"
#include "Engine/MeterEngine.h"
#include "Engine/SampleRingBuffer.h"

using namespace juce;

//==============================================================================
class MultiMeterAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    MultiMeterAudioProcessor();
    ~MultiMeterAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Manages the state of all parameters in the audio processor
    juce::AudioProcessorValueTreeState apvts;

    // Measures peak, RMS and correlation on the audio thread
    MeterEngine meterEngine;

    // Carries every sample to the goniometer and the spectrum analyzer
    SampleRingBuffer sampleRingBuffer;

#if USE_OSC
    // Oscillator for generating test signals
    juce::dsp::Oscillator<float> osc {[](float x) { return std::sin(x); }};

    // Gain control for the oscillator output
    juce::dsp::Gain<float> gain;
#endif

private:
    // Applies the settings of a state saved by version 1
    void applyLegacyState(const Parameters::LegacyState& state);

    // The averager duration, which sets the slow correlation's integration time
    std::atomic<float>* averagerDurationParameter = nullptr;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultiMeterAudioProcessor)
};
