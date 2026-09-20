/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.
    This project is built with JUCE version 9.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Constructor for MultiMeterAudioProcessorEditor class.
// Initializes GUI components and attaches the controls to their parameters.
MultiMeterAudioProcessorEditor::MultiMeterAudioProcessorEditor(MultiMeterAudioProcessor& p) :
    AudioProcessorEditor(&p),
    audioProcessor(p),
    spectrumSource(audioProcessor),
    spectrumAnalyzer(audioProcessor.apvts, spectrumSource),
    spectrogram(spectrumSource),
    scaleKnobSlider(*audioProcessor.apvts.getParameter(Parameters::ID::goniometerScale), "%"),
    scaleKnobSliderAttachment(audioProcessor.apvts, Parameters::ID::goniometerScale, scaleKnobSlider),
    tickDisplayAttachment(audioProcessor.apvts, Parameters::ID::showTick, tickDisplay),
    mainViewAttachment(*audioProcessor.apvts.getParameter(Parameters::ID::mainView),
        [this](float value) { showMainView(juce::roundToInt(value)); }),
    meterViewAttachment(*audioProcessor.apvts.getParameter(Parameters::ID::meterView),
        [this](float value) { meterViewButton.setSelection(juce::roundToInt(value)); }),
    histogramViewAttachment(*audioProcessor.apvts.getParameter(Parameters::ID::histogramView),
        [this](float value) { layoutHistograms(juce::roundToInt(value)); }),
    vBlankAttachment(this, [this](double timestampSeconds) { vBlank(timestampSeconds); })
{
    auto& apvts = audioProcessor.apvts;
    // The menu switch changes between the visuals
    addAndMakeVisible(menuViewSwitch);
    menuViewSwitch.setOptions(Parameters::mainViewNames);
    menuViewSwitch.onChange = [this](int id) { mainViewAttachment.setValueAsCompleteGesture((float)id); };

    // Histogram view button setup
    addAndMakeVisible(histogramViewButton);
    for (auto& name : Parameters::histogramViewNames)
        histogramViewButton.addOption(name);
    histogramViewButton.onChange = [this](int id) { histogramViewAttachment.setValueAsCompleteGesture((float)id); };

    // Meter setup
    addAndMakeVisible(peakMeter);
    addAndMakeVisible(RMSMeter);
    addChildComponent(peakHistogram);
    addChildComponent(rmsHistogram);
    addChildComponent(gonioMeter);
    addAndMakeVisible(correlationMeter);
    addChildComponent(spectrumAnalyzer);
    addChildComponent(spectrogram);
    addChildComponent(loudnessView);

    // The options of the views, each of which shows with the views that it belongs to
    {
        using namespace Parameters;
        addAndMakeVisible(optionsRow);

        optionsRow.addChoice(OptionsRow::views({ goniometerView }), apvts, ID::goniometerMode, 100);
        optionsRow.addChoice(OptionsRow::views({ goniometerView }), apvts, ID::goniometerPersistence, 140);

        optionsRow.addChoice(OptionsRow::views({ analyzerView }), apvts, ID::spectrumChannels, 96);
        optionsRow.addChoice(OptionsRow::views({ analyzerView, spectrogramView }), apvts, ID::spectrumTilt, 112);
        optionsRow.addChoice(OptionsRow::views({ analyzerView }), apvts, ID::spectrumSmoothing, 104);
        optionsRow.addChoice(OptionsRow::views({ analyzerView, spectrogramView }), apvts, ID::spectrumResolution, 84);
        optionsRow.addToggle(OptionsRow::views({ analyzerView }), apvts, ID::spectrumPeakHold, "Hold", 44);

        // Freezing is for a moment's look, so it is not a setting that is saved
        freezeButton = &optionsRow.addButton(OptionsRow::views({ analyzerView, spectrogramView }), "Freeze", 52, true, [] {});

        optionsRow.addChoice(OptionsRow::views({ Parameters::loudnessView }), apvts, ID::loudnessTarget, 170);
        optionsRow.addButton(OptionsRow::views({ Parameters::loudnessView }), "Reset", 60, false, [this]
        {
            audioProcessor.resetLoudness();
            loudnessView.clearHistory();
        });
    }

    // Scale knob setup
    addAndMakeVisible(scaleKnobSlider);
    addLabel(scaleKnobLabel, "Goniometer Scale");

    // Level meter decay setup
    addAndMakeVisible(levelMeterDecaySelector);
    levelMeterDecaySelector.addItemList(Parameters::decayRateNames, 1);
    levelMeterDecayAttachment = std::make_unique<APVTS::ComboBoxAttachment>(apvts, Parameters::ID::decayRate, levelMeterDecaySelector);
    addLabel(levelMeterDecayLabel, "Level Meter Decay");

    // Averager duration setup
    addAndMakeVisible(averagerDurationSelector);
    averagerDurationSelector.addItemList(Parameters::averagerDurationNames, 1);
    averagerDurationAttachment = std::make_unique<APVTS::ComboBoxAttachment>(apvts, Parameters::ID::averagerDuration, averagerDurationSelector);
    addLabel(averagerDurationLabel, "Averager Duration");

    // Meter view setup
    addAndMakeVisible(meterViewButton);
    for (auto& name : Parameters::meterViewNames)
        meterViewButton.addOption(name);
    meterViewButton.onChange = [this](int id) { meterViewAttachment.setValueAsCompleteGesture((float)id); };
    addLabel(meterViewLabel, "Level Meter Display");

    // Tick display setup
    addAndMakeVisible(tickDisplay);
    addLabel(tickDisplayLabel, "Tick Display");

    // Hold time setup
    addAndMakeVisible(holdTimeSelector);
    holdTimeSelector.addItemList(Parameters::holdTimeNames, 1);
    holdTimeAttachment = std::make_unique<APVTS::ComboBoxAttachment>(apvts, Parameters::ID::holdTime, holdTimeSelector);
    addLabel(holdTimeLabel, "Tick Hold Duration");

    // Reset hold setup
    // The button is only needed while the ticks are held forever
    addChildComponent(resetHold);
    resetHold.setClickingTogglesState(false);
    resetHold.onClick = [this] { resetHoldRequested = true; };

    // Histogram view setup
    addLabel(histogramViewLabel, "Histogram Display");
    addLabel(correlationLabel0, "-1");
    addLabel(correlationLabel1, "0");
    addLabel(correlationLabel2, "+1");

    // Set the look and feel
    setLookAndFeel(&lookAndFeel);

    // Set the initial size of the editor
    setSize(800, 430);

    // Bring the custom controls in line with their parameters, now that the layout is known
    mainViewAttachment.sendInitialUpdate();
    meterViewAttachment.sendInitialUpdate();
    histogramViewAttachment.sendInitialUpdate();

    // Discard the peaks that built up while the editor was closed
    audioProcessor.meterEngine.read();
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

    menuViewSwitch.setBounds(0, 0, getWidth(), 20);

    auto visualsRoom = getLocalBounds();
    visualsRoom.removeFromTop(20);
    auto meterRoom  = visualsRoom.removeFromRight(getWidth() / 3);
    auto controlRoom = visualsRoom.removeFromBottom(95);

    // The options of the view sit between the view and the controls
    optionsRow.setBounds(visualsRoom.removeFromBottom(30).reduced(14, 2));
    auto correlationRoom = meterRoom.removeFromBottom(meterRoom.getHeight() / 5);

    auto stackedSpace = visualsRoom.reduced(26,20);
    peakStacked = stackedSpace.removeFromTop(stackedSpace.getHeight() / 2).withTrimmedBottom(5);
    rmsStacked = stackedSpace.withTrimmedTop(5);

    auto sbsSpace = visualsRoom.reduced(26, 20);
    peakSBS = sbsSpace.removeFromLeft(sbsSpace.getWidth() / 2).withTrimmedRight(5);
    rmsSBS = sbsSpace.withTrimmedLeft(5);

    // Visualizers
    spectrumAnalyzer.setBounds(visualsRoom.reduced(20));
    spectrogram.setBounds(visualsRoom.reduced(26, 20));
    loudnessView.setBounds(visualsRoom.reduced(26, 20));
    gonioMeter.setBounds(visualsRoom.getCentreX() - gonioMeterWidth / 2, visualsRoom.getCentreY() - gonioMeterWidth / 2, gonioMeterWidth, gonioMeterWidth);

    layoutHistograms(histogramViewButton.getSelectedId());

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

void MultiMeterAudioProcessorEditor::vBlank(double timestampSeconds)
{
    if (lastUpdateTime < 0.0)
    {
        lastUpdateTime = timestampSeconds;
        lastAudioTime = timestampSeconds;
        return;
    }

    // Displays that refresh faster than the update rate skip the frames in between
    // The small tolerance keeps a 60 Hz display from dropping frames to timing jitter
    const double elapsedSeconds = timestampSeconds - lastUpdateTime;
    if (elapsedSeconds < 0.9 / updateRateHz)
        return;

    lastUpdateTime = timestampSeconds;

    // After a long pause, such as the window being hidden, the meters carry on rather than jump
    updateMeters((float)juce::jmin(elapsedSeconds, 0.1));
}

void MultiMeterAudioProcessorEditor::updateMeters(float elapsedSeconds)
{
    // The measurements were made on the audio thread from every sample
    auto readings = audioProcessor.meterEngine.read();

    // When the host stops calling the processor the last readings would stay forever,
    // so they are replaced with silence once no audio has arrived for a while
    const auto totalWritten = audioProcessor.sampleRingBuffer.getTotalWritten();
    if (totalWritten != lastTotalWritten)
    {
        lastTotalWritten = totalWritten;
        lastAudioTime = lastUpdateTime;
    }

    const bool audioRunning = lastUpdateTime - lastAudioTime <= silenceTimeoutSeconds;
    if (!audioRunning)
        readings = {};

    // Convert the readings to decibels
    // The 2nd parameter of juce::Decibels::gainToDecibels() defines what "negative infinity" is
    float leftChannelMagnitudeDecibels = juce::Decibels::gainToDecibels(readings.peak[0], NEGATIVE_INFINITY);
    float rightChannelMagnitudeDecibels = juce::Decibels::gainToDecibels(readings.peak[1], NEGATIVE_INFINITY);
    float leftChannelRMSDecibels = juce::Decibels::gainToDecibels(readings.rms[0], NEGATIVE_INFINITY);
    float rightChannelRMSDecibels = juce::Decibels::gainToDecibels(readings.rms[1], NEGATIVE_INFINITY);

    // The settings come straight from the parameters
    using namespace Parameters;

    LevelMeterSettings meterSettings;
    meterSettings.decayRateDbPerSecond = valueAt(decayRatesDbPerSecond, getChoice(ID::decayRate));
    meterSettings.holdTimeSeconds = valueAt(holdTimesSeconds, getChoice(ID::holdTime));
    meterSettings.viewId = getChoice(ID::meterView);
    meterSettings.showTick = isOn(ID::showTick);
    meterSettings.resetHold = resetHoldRequested;
    resetHoldRequested = false;

    // The reset button is only needed while the ticks are held forever
    resetHold.setVisible(std::isinf(meterSettings.holdTimeSeconds));

    peakMeter.update(leftChannelMagnitudeDecibels, rightChannelMagnitudeDecibels, meterSettings, elapsedSeconds);
    RMSMeter.update(leftChannelRMSDecibels, rightChannelRMSDecibels, meterSettings, elapsedSeconds);

    // Updating peak and RMS histograms with the average of left and right channel RMS and peak values
    // They keep recording while another view is shown
    peakHistogram.update((leftChannelMagnitudeDecibels + rightChannelMagnitudeDecibels) / 2);
    rmsHistogram.update((leftChannelRMSDecibels + rightChannelRMSDecibels) / 2);

    correlationMeter.update(readings.correlationFast, readings.correlationSlow);

    // The loudness and the true peak are read in every update, whichever view is showing,
    // because reading the true peak is what restarts its measurement
    {
        auto loudness = audioProcessor.loudnessMeter.read();
        const auto truePeak = audioProcessor.truePeakDetector.read();

        // The momentary and short-term loudness describe the present, so they go quiet with the audio.
        // The integrated loudness and the range describe the programme so far, so they stay.
        if (!audioRunning)
            loudness.momentary = loudness.shortTerm = LoudnessMeter::silence;

        const float truePeakDb = juce::Decibels::gainToDecibels(juce::jmax(truePeak.peak[0], truePeak.peak[1]), NEGATIVE_INFINITY);
        const float maxTruePeakDb = juce::Decibels::gainToDecibels(juce::jmax(truePeak.maxPeak[0], truePeak.maxPeak[1]), NEGATIVE_INFINITY);
        const float target = valueAt(loudnessTargetsLufs, getChoice(ID::loudnessTarget));

        loudnessView.update(loudness, audioRunning ? truePeakDb : NEGATIVE_INFINITY, maxTruePeakDb, target, audioRunning, elapsedSeconds);
    }

    // Only the visible view needs the samples themselves
    if (gonioMeter.isVisible())
    {
        // Scaling knob values are mapped to a range of 50 - 200
        // This value is used as a gain factor in the updateCoeff function of the gonioMeter
        gonioMeter.updateCoeff(getValue(ID::goniometerScale) / 100.f);

        const auto mode = getChoice(ID::goniometerMode) == polarMode ? Goniometer::polar : Goniometer::lissajous;
        const float persistence = valueAt(goniometerPersistenceSeconds, getChoice(ID::goniometerPersistence));
        gonioMeter.update(audioProcessor.sampleRingBuffer, elapsedSeconds, mode, persistence);
    }

    if (spectrumAnalyzer.isVisible() || spectrogram.isVisible())
    {
        // While frozen the spectra stay as they are, but a change of setting still shows
        const bool frozen = freezeButton != nullptr && freezeButton->getToggleState();
        const int order = valueAt(spectrumResolutionOrders, getChoice(ID::spectrumResolution));
        const float tilt = valueAt(spectrumTiltsDbPerOctave, getChoice(ID::spectrumTilt));
        const bool hasNewSpectra = !frozen && spectrumSource.update(elapsedSeconds, order);

        if (spectrumAnalyzer.isVisible())
        {
            SpectrumAnalyzer::Settings analyzerSettings;
            analyzerSettings.midSide = getChoice(ID::spectrumChannels) == 1;
            analyzerSettings.tiltDbPerOctave = tilt;
            analyzerSettings.smoothingOctaves = valueAt(spectrumSmoothingOctaves, getChoice(ID::spectrumSmoothing));
            analyzerSettings.peakHold = isOn(ID::spectrumPeakHold);
            spectrumAnalyzer.update(hasNewSpectra, analyzerSettings);
        }
        else if (hasNewSpectra)
        {
            spectrogram.addColumn(tilt);
        }
    }
}

float MultiMeterAudioProcessorEditor::getValue(const juce::String& parameterID) const
{
    return audioProcessor.apvts.getRawParameterValue(parameterID)->load();
}

int MultiMeterAudioProcessorEditor::getChoice(const juce::String& parameterID) const
{
    return juce::roundToInt(getValue(parameterID));
}

bool MultiMeterAudioProcessorEditor::isOn(const juce::String& parameterID) const
{
    return getValue(parameterID) > 0.5f;
}

void MultiMeterAudioProcessorEditor::addLabel(juce::Label& label, const juce::String& text)
{
    addAndMakeVisible(label);
    label.setText(text, juce::NotificationType::dontSendNotification);
    label.setColour(Label::ColourIds::textColourId, Colours::black);
}

void MultiMeterAudioProcessorEditor::showMainView(int viewId)
{
    menuViewSwitch.setSelection(viewId);

    // Based on the view ID one of the visuals is set to visible and the others are hidden
    gonioMeter.setVisible(viewId == Parameters::goniometerView);
    spectrumAnalyzer.setVisible(viewId == Parameters::analyzerView);
    spectrogram.setVisible(viewId == Parameters::spectrogramView);
    loudnessView.setVisible(viewId == Parameters::loudnessView);
    optionsRow.showView(viewId);
    peakHistogram.setVisible(viewId == Parameters::histogramView);
    rmsHistogram.setVisible(viewId == Parameters::histogramView);
}

void MultiMeterAudioProcessorEditor::layoutHistograms(int histogramViewId)
{
    histogramViewButton.setSelection(histogramViewId);

    peakHistogram.setBounds(histogramViewId ? peakStacked : peakSBS);
    rmsHistogram.setBounds(histogramViewId ? rmsStacked : rmsSBS);
}
