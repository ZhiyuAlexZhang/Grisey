/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.
    This project is created using JUCE version 6.1.2.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Implementation for the GonioGlanceAudioProcessorEditor class.
// Constructor for MultiMeterAudioProcessorEditor class.
// Initializes GUI components and sets up listeners.
MultiMeterAudioProcessorEditor::MultiMeterAudioProcessorEditor(MultiMeterAudioProcessor& p) :
    AudioProcessorEditor(&p),
    audioProcessor(p),
    gonioMeter(buffer),
    correlationMeter(buffer, audioProcessor.getSampleRate()),
    spectrumAnalyzer(audioProcessor),
    scaleKnobSlider(*audioProcessor.apvts.getParameter("Scale Knob"), "%"),
    scaleKnobSliderAttachment(audioProcessor.apvts, "Scale Knob", scaleKnobSlider)
{
    // Timer setup
    startTimerHz(60);
    buffer.clear();

    // add menu view switch and also add listener so editor can use callback to switch between three views
    // menu switch is simply added to switch between three different visuals (goniometer, spectrum analyzer and histogram)
    addAndMakeVisible(menuViewSwitch);
    menuViewSwitch.addListener(*this);

    // Histogram view button setup
    addAndMakeVisible(histogramViewButton);
    histogramViewButton.addOption("Parallel", *this);
    histogramViewButton.addOption("Stacked", *this);
    //before setting the selection to the histogramview combobox make sure it is within valid range, if not then set to default
    int histoID = (audioProcessor.histogramDisplayID > 1 || audioProcessor.histogramDisplayID < 0) ? 0 : audioProcessor.histogramDisplayID;
    histogramViewButton.setSelection(histoID);

    // Meter setup
    addAndMakeVisible(peakMeter);
    addAndMakeVisible(RMSMeter);
    addChildComponent(peakHistogram);
    addChildComponent(rmsHistogram);
    addChildComponent(gonioMeter);
    addAndMakeVisible(correlationMeter);
    addAndMakeVisible(spectrumAnalyzer);

    // Scale knob setup
    addAndMakeVisible(scaleKnobSlider);
    scaleKnobSlider.addListener(this);
    // always check if the value is in valud range then set the value
    float validValue = (audioProcessor.sliderValue > 200 || audioProcessor.sliderValue < 50) ? 100 : audioProcessor.sliderValue;
    scaleKnobSlider.setValue(validValue, juce::dontSendNotification);
    addAndMakeVisible(scaleKnobLabel);
    scaleKnobLabel.setText("Goniometer Scale", juce::NotificationType::dontSendNotification);
    scaleKnobLabel.setColour(Label::ColourIds::textColourId, Colours::black);

    // Level meter decay setup
    addAndMakeVisible(levelMeterDecaySelector);
    levelMeterDecaySelector.addItemList(juce::StringArray("-3dB/s", "-6dB/s", "-12dB/s", "-24dB/s", "-36dB/s"), 1);
    levelMeterDecaySelector.addListener(this);
    int validID = (audioProcessor.levelMeterDecayId > 5 || audioProcessor.levelMeterDecayId < 1) ? 1 : audioProcessor.levelMeterDecayId;
    levelMeterDecaySelector.setSelectedId(validID, juce::dontSendNotification);
    addAndMakeVisible(levelMeterDecayLabel);
    levelMeterDecayLabel.setText("Level Meter Decay", juce::NotificationType::dontSendNotification);
    levelMeterDecayLabel.setColour(Label::ColourIds::textColourId, Colours::black);

    // Averager duration setup
    addAndMakeVisible(averagerDurationSelector);
    averagerDurationSelector.addItemList(juce::StringArray("100ms", "250ms", "500ms", "1000ms", "2000ms"), 1);
    averagerDurationSelector.addListener(this);
    validID = (audioProcessor.averagerDurationId > 5 || audioProcessor.averagerDurationId < 1) ? 1 : audioProcessor.averagerDurationId;
    averagerDurationSelector.setSelectedId(validID, juce::dontSendNotification);
    addAndMakeVisible(averagerDurationLabel);
    averagerDurationLabel.setText("Averager Duration", juce::NotificationType::dontSendNotification);
    averagerDurationLabel.setColour(Label::ColourIds::textColourId, Colours::black);

    // Meter view setup
    addAndMakeVisible(meterViewButton);
    meterViewButton.addOption("Both", *this);
    meterViewButton.addOption("Peak", *this);
    meterViewButton.addOption("Avg", *this);
    int meterID = (audioProcessor.levelMeterDisplayID > 2 || audioProcessor.levelMeterDisplayID < 0) ? 0 : audioProcessor.levelMeterDisplayID;
    meterViewButton.setSelection(meterID);
    addAndMakeVisible(meterViewLabel);
    meterViewLabel.setText("Level Meter Display", juce::NotificationType::dontSendNotification);
    meterViewLabel.setColour(Label::ColourIds::textColourId, Colours::black);

    // Tick display setup
    addAndMakeVisible(tickDisplay);
    tickDisplay.addListener(this);
    tickDisplay.setToggleState(true, juce::dontSendNotification);
    tickDisplay.clicked();
    addAndMakeVisible(tickDisplayLabel);
    tickDisplayLabel.setText("Tick Display", juce::NotificationType::dontSendNotification);
    tickDisplayLabel.setColour(Label::ColourIds::textColourId, Colours::black);

    // Hold time setup
    addAndMakeVisible(holdTimeSelector);
    holdTimeSelector.addItemList(juce::StringArray("0s", "0.5s", "2s", "4s", "6s", "inf"), 1);
    holdTimeSelector.addListener(this);
    validID = (audioProcessor.holdTimeId > 6 || audioProcessor.holdTimeId < 1) ? 3 : audioProcessor.holdTimeId;
    holdTimeSelector.setSelectedId(validID, juce::dontSendNotification);
    addAndMakeVisible(holdTimeLabel);
    holdTimeLabel.setText("Tick Hold Duration", juce::NotificationType::dontSendNotification);
    holdTimeLabel.setColour(Label::ColourIds::textColourId, Colours::black);

    // Reset hold setup
    addAndMakeVisible(resetHold);
    resetHold.clicked();
    resetHold.setVisible(holdTimeSelector.getSelectedId() == 6);

    // Histogram view setup
    addAndMakeVisible(histogramViewLabel);
    histogramViewLabel.setText("Histogram Display", juce::NotificationType::dontSendNotification);
    histogramViewLabel.setColour(Label::ColourIds::textColourId, Colours::black);
    addAndMakeVisible(correlationLabel0);
    correlationLabel0.setText("-1", juce::NotificationType::dontSendNotification);
    correlationLabel0.setColour(Label::ColourIds::textColourId, Colours::black);
    addAndMakeVisible(correlationLabel1);
    correlationLabel1.setText("0", juce::NotificationType::dontSendNotification);
    correlationLabel1.setColour(Label::ColourIds::textColourId, Colours::black);
    addAndMakeVisible(correlationLabel2);
    correlationLabel2.setText("+1", juce::NotificationType::dontSendNotification);
    correlationLabel2.setColour(Label::ColourIds::textColourId, Colours::black);

    // Set the look and feel
    setLookAndFeel(&lookAndFeel);

    // Set the initial size of the editor
    setSize(800, 400);
}


MultiMeterAudioProcessorEditor::~MultiMeterAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void MultiMeterAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Fill the background with a solid color
    g.fillAll(BACKGROUND_COLOR);

}

void MultiMeterAudioProcessorEditor::resized()
{
    const int gonioMeterWidth = 285;

    menuViewSwitch.setBounds(0, 0, 800, 20);

    auto visualsRoom = getBounds();
    visualsRoom.removeFromTop(20);
    auto meterRoom  = visualsRoom.removeFromRight(getWidth() / 3);
    auto controlRoom = visualsRoom.removeFromBottom(visualsRoom.getHeight() / 4);
    auto correlationRoom = meterRoom.removeFromBottom(meterRoom.getHeight() / 5);

    auto stackedSpace = visualsRoom.reduced(26,20);
    peakStacked = stackedSpace.removeFromTop(stackedSpace.getHeight() / 2).withTrimmedBottom(5);
    rmsStacked = stackedSpace.withTrimmedTop(5);

    auto sbsSpace = visualsRoom.reduced(26, 20);
    peakSBS = sbsSpace.removeFromLeft(sbsSpace.getWidth() / 2).withTrimmedRight(5);
    rmsSBS = sbsSpace.withTrimmedLeft(5);

    // Visualizers
    spectrumAnalyzer.setBounds(visualsRoom.reduced(20));
    gonioMeter.setBounds(visualsRoom.getCentreX() - gonioMeterWidth / 2, visualsRoom.getCentreY() - gonioMeterWidth / 2, gonioMeterWidth, gonioMeterWidth);

    peakHistogram.setBounds(audioProcessor.histogramDisplayID ? peakStacked : peakSBS);
    rmsHistogram.setBounds(audioProcessor.histogramDisplayID ? rmsStacked : rmsSBS);

    auto peakSection = meterRoom.removeFromLeft(meterRoom.getWidth() / 2).reduced(10,0);
    peakMeter.setBounds(peakSection.expanded(0, 5).translated(0,25));
    RMSMeter.setBounds(meterRoom.reduced(10,0).expanded(0, 5).translated(0,25));

    correlationMeter.setBounds(correlationRoom.reduced(13,30).translated(0,2));

    int y = correlationMeter.getBottom();
    int wd = 24;
    int ht = 24;
    correlationLabel0.setBounds(correlationMeter.getX(),y, wd, ht);
    correlationLabel2.setBounds(correlationMeter.getRight()-wd, y, wd, ht);
    correlationLabel1.setBounds(correlationMeter.getX() + correlationMeter.getWidth()/2-12, y, wd, ht);

    // Menu Controls

    auto delY = 22;

    auto tempspace = controlRoom.getWidth() / 5;
    auto knobSpace = controlRoom.removeFromLeft(tempspace);
    auto knobLabel = knobSpace.removeFromTop(delY);

    scaleKnobLabel.setBounds(knobLabel);
    scaleKnobSlider.setBounds(knobSpace.expanded(5).translated(0,5));

    auto levelLabel = controlRoom.removeFromLeft(120);
    auto levelSpace = controlRoom.removeFromLeft(159);

    meterViewLabel.setBounds(levelLabel.removeFromTop(delY));
    meterViewButton.setBounds(levelSpace.removeFromTop(delY));

    levelMeterDecayLabel.setBounds(levelLabel.removeFromTop(delY));
    levelMeterDecaySelector.setBounds(levelSpace.removeFromTop(delY).reduced(0,1));

    tickDisplayLabel.setBounds(levelLabel.removeFromTop(delY));
    tickDisplay.setBounds(levelSpace.removeFromTop(delY).reduced(0,2));

    holdTimeLabel.setBounds(levelLabel.removeFromTop(delY));
    auto tickSpace1 = levelSpace.removeFromTop(delY);
    auto tickSpace = tickSpace1.removeFromRight(80);
    holdTimeSelector.setBounds(tickSpace1.reduced(0, 1).withTrimmedRight(2));
    resetHold.setBounds(tickSpace.reduced(0,1).withTrimmedLeft(2));

    controlRoom.removeFromLeft(10);
    auto space1 = controlRoom.removeFromRight(130);
    histogramViewLabel.setBounds(space1.removeFromTop(delY).withTrimmedRight(10));
    histogramViewButton.setBounds(space1.removeFromTop(delY).reduced(0, 1).translated(5,0));

    averagerDurationLabel.setBounds(space1.removeFromTop(delY).withTrimmedRight(10));
    averagerDurationSelector.setBounds(space1.removeFromTop(delY).removeFromLeft(104).reduced(0, 1).translated(6,0));
}

void MultiMeterAudioProcessorEditor::timerCallback()
{
    auto& audioProcessorFifo = audioProcessor.fifo;

    // If the audioProcessor.fifo has items available for reading
    if (/*JUCE_WINDOWS ||*/ audioProcessorFifo.getNumAvailableForReading() > 0)
    {
        // Use a while( fifo.pull(buffer) ) loop to pull every element available out of the fifo
        while (audioProcessorFifo.pull(buffer))
        {
        }
        
        gonioMeter.update(buffer);
    }
    
    // After finishing pulling all buffers out of the fifo in timerCallback,
    // use the buffer’s member function that returns the magnitude for a channel to get
    // the Left channel’s magnitude
    // This function returns a "gain" value.f
    float leftChannelMagnitudeRaw = buffer.getMagnitude(0, 0, buffer.getNumSamples());
    float rightChannelMagnitudeRaw = buffer.getMagnitude(1, 0, buffer.getNumSamples());
    
    float leftChannelRMSRaw = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
    float rightChannelRMSRaw = buffer.getRMSLevel(1, 0, buffer.getNumSamples());

    // Convert this value to decibels
    // The juce::Decibels::gainToDecibels() function takes a 2nd parameter
    // This 2nd parameter lets you define what "negative infinity" is, which is NEGATIVE_INFINITY
    float leftChannelMagnitudeDecibels = juce::Decibels::gainToDecibels(leftChannelMagnitudeRaw,
                                                                        NEGATIVE_INFINITY);
    float rightChannelMagnitudeDecibels = juce::Decibels::gainToDecibels(rightChannelMagnitudeRaw,
                                                                         NEGATIVE_INFINITY);
    
    float leftChannelRMSDecibels = juce::Decibels::gainToDecibels(leftChannelRMSRaw,
                                                                  NEGATIVE_INFINITY);
    float rightChannelRMSDecibels = juce::Decibels::gainToDecibels(rightChannelRMSRaw,
                                                                  NEGATIVE_INFINITY);
    
    // In this section, control values are updated using the TimerCallback mechanism

    // The update function is called with the latest values for peak and RMS meters, as well as other parameters
    // These parameters are updated via corresponding listener functions, such as ComboBoxChanged or SliderValueChanged
    peakMeter.update(leftChannelMagnitudeDecibels, rightChannelMagnitudeDecibels, currentDecayRate, audioProcessor.levelMeterDisplayID, tickDisplay.getToggleState(), holdTime, resetHold.getToggleState());
    RMSMeter.update(leftChannelRMSDecibels, rightChannelRMSDecibels, currentDecayRate, audioProcessor.levelMeterDisplayID, tickDisplay.getToggleState(), holdTime, resetHold.getToggleState());

    // Resetting the resetHold button to false state to prevent it from toggling
    if (resetHold.getToggleState())
        resetHold.setToggleState(false, juce::dontSendNotification);

    // Updating peak and RMS histograms with the average of left and right channel RMS and peak values
    peakHistogram.update((leftChannelMagnitudeDecibels + rightChannelMagnitudeDecibels) / 2);
    rmsHistogram.update((leftChannelRMSDecibels + rightChannelRMSDecibels) / 2);

    // Updating the correlation averager with the new duration every time the TimerCallback is invoked
    correlationMeter.update(averagerDuration);

    // Scaling knob values are mapped to a range of 50 - 200
    // This value is used as a gain factor in the updateCoeff function of the gonioMeter
    float gain = scaleKnobSlider.getValue() / 100;
    gonioMeter.updateCoeff(gain); // Scaling the gonioMeter plot

    // Triggering a repaint for the gonioMeter
    gonioMeter.repaint();
}

void MultiMeterAudioProcessorEditor::comboBoxChanged(juce::ComboBox* comboBox)
{
    // This function handles changes in ComboBox controls

    // Modifying variables based on the selected ComboBox item
    if (comboBox == &levelMeterDecaySelector)
    {
        // Extracting the decay rate value from the ComboBox text and storing it
        currentDecayRate = -comboBox->getItemText(comboBox->getSelectedId() - 1).removeCharacters("dB/s").getFloatValue();

        // Storing the selected ID in the value tree for retrieval by subsequent instances
        audioProcessor.levelMeterDecayId = comboBox->getSelectedId();
    }

    // Updating variables and storing the selected ID in the value tree for averager duration
    else if (comboBox == &averagerDurationSelector)
    {
        averagerDuration = comboBox->getItemText(comboBox->getSelectedId() - 1).removeCharacters("ms").getFloatValue(); //ms
        audioProcessor.averagerDurationId = comboBox->getSelectedId();
    }
    else if (comboBox == &holdTimeSelector)
    {
        // Adjusting hold time value based on ComboBox selection
        if (comboBox->getNumItems() == comboBox->getSelectedId())
        {
            holdTime = 60; // Assuming 60 seconds for infinite hold time
            resetHold.setVisible(true);
        }
        else
        {
            holdTime = comboBox->getItemText(comboBox->getSelectedId() - 1).removeCharacters("s").getFloatValue();
            resetHold.setVisible(false);
        }

        // Converting hold time to milliseconds and storing the selected ID in the value tree
        holdTime *= 1000;
        audioProcessor.holdTimeId = comboBox->getSelectedId();
    }
}

void MultiMeterAudioProcessorEditor::buttonClicked(juce::Button* button)
{
    if (button == &tickDisplay)
    {
        // This variable is used on plugin set and get state to load and retrieve the tick show/hide state
        audioProcessor.tickDisplayState = button->getToggleState();
    }
    else
    {
        // All the buttonclicked callback are called so that they get's updated
        menuViewSwitch.buttonClicked(button);
        histogramViewButton.buttonClicked(button);
        meterViewButton.buttonClicked(button);

        // After buttunclicked is called for menuswitch it's id will be updated internally and can be used
        // to switch which visual to show on UI
        int id = menuViewSwitch.getSwitchID();

        // Based on the updated id value one of the visual is set to visible and other are hide
        spectrumAnalyzer.setVisible(id == 1);
        peakHistogram.setVisible(id == 2);
        rmsHistogram.setVisible(id == 2);
        gonioMeter.setVisible(id == 0);

        // After buttonclicked is called levelmeter id and histogrami id are updated to use it for later
        audioProcessor.levelMeterDisplayID = meterViewButton.getSelectedId();
        audioProcessor.histogramDisplayID = histogramViewButton.getSelectedId();
    }

    // Histogram view is swtiched whenever the button is clicked 
    peakHistogram.setBounds(audioProcessor.histogramDisplayID ? peakStacked : peakSBS);
    rmsHistogram.setBounds(audioProcessor.histogramDisplayID ? rmsStacked : rmsSBS);
}

void MultiMeterAudioProcessorEditor::sliderValueChanged(juce::Slider* slider)
{
    // This callback is dedicated to the scaleKnobSlider to store the value of slider in valueTree
    if (slider == &scaleKnobSlider)
    {
        audioProcessor.sliderValue = slider->getValue();
    }
}
