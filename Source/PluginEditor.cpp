/*
  ==============================================================================

    The editor of the plugin: a header with a tab for each view, the view itself,
    a column of meters that is always showing, and a bar of controls for the view.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // The size of the editor is kept in the state, beside the parameters, so that it is saved with the session
    const juce::Identifier editorWidthProperty { "editorWidth" };
    const juce::Identifier editorHeightProperty { "editorHeight" };
}

//==============================================================================
MultiMeterAudioProcessorEditor::MultiMeterAudioProcessorEditor(MultiMeterAudioProcessor& p) :
    AudioProcessorEditor(&p),
    audioProcessor(p),
    spectrumSource(audioProcessor),
    goniometerView(audioProcessor.apvts, Parameters::ID::goniometerScale),
    spectrumView(spectrumSource),
    spectrogramView(spectrumSource),
    mainViewAttachment(*audioProcessor.apvts.getParameter(Parameters::ID::mainView),
        [this](float value) { showMainView(juce::roundToInt(value)); }),
    vBlankAttachment(this, [this](double timestampSeconds) { vBlank(timestampSeconds); })
{
    using namespace Parameters;
    auto& apvts = audioProcessor.apvts;

    setLookAndFeel(&lookAndFeel);
    setOpaque(true);

    // The tabs change between the views
    addAndMakeVisible(tabs);
    tabs.setTabs(mainViewNames);
    tabs.onChange = [this](int id) { mainViewAttachment.setValueAsCompleteGesture((float)id); };

    addChildComponent(goniometerView);
    addChildComponent(spectrumView);
    addChildComponent(spectrogramView);
    addChildComponent(historyView);
    addChildComponent(loudnessView);

    addAndMakeVisible(levelMeters);
    addAndMakeVisible(loudnessSummary);
    addAndMakeVisible(correlationBar);

    // The controls of the views, each of which shows with the views that it belongs to
    addAndMakeVisible(controlBar);

    controlBar.addMenu(ControlBar::views({ viewGoniometer }), apvts, ID::goniometerMode);
    controlBar.addMenu(ControlBar::views({ viewGoniometer }), apvts, ID::goniometerPersistence);

    controlBar.addMenu(ControlBar::views({ viewSpectrum }), apvts, ID::spectrumChannels);
    controlBar.addMenu(ControlBar::views({ viewSpectrum, viewSpectrogram }), apvts, ID::spectrumTilt);
    controlBar.addMenu(ControlBar::views({ viewSpectrum }), apvts, ID::spectrumSmoothing);
    controlBar.addMenu(ControlBar::views({ viewSpectrum, viewSpectrogram }), apvts, ID::spectrumResolution);
    controlBar.addToggle(ControlBar::views({ viewSpectrum }), apvts, ID::spectrumPeakHold, "Peak hold");

    // Freezing is for a moment's look, so it is not a setting that is saved
    freezeButton = &controlBar.addButton(ControlBar::views({ viewSpectrum, viewSpectrogram }), "Freeze", true, [] {});

    controlBar.addMenu(ControlBar::views({ viewLoudness }), apvts, ID::loudnessTarget, "TARGET");
    controlBar.addButton(ControlBar::views({ viewLoudness }), "Reset", false, [this]
    {
        audioProcessor.resetLoudness();
        loudnessView.clearHistory();
    });

    // The settings of the side column's meters share one menu
    addAndMakeVisible(meterSettingsButton);
    meterSettingsButton.buildMenu = [this](juce::PopupMenu& menu) { buildMeterSettingsMenu(menu); };

    // The editor opens at the size that it was last given. The size is read first, because setting
    // the limits already resizes the editor, which would otherwise be taken for the user's choice.
    const int savedWidth = apvts.state.getProperty(editorWidthProperty, defaultWidth);
    const int savedHeight = apvts.state.getProperty(editorHeightProperty, defaultHeight);

    setResizable(true, true);
    setResizeLimits(minWidth, minHeight, maxWidth, maxHeight);
    setSize(juce::jlimit(minWidth, maxWidth, savedWidth), juce::jlimit(minHeight, maxHeight, savedHeight));
    isConstructed = true;

    // Show the view that the parameter selects, now that the layout is known
    mainViewAttachment.sendInitialUpdate();

    // Discard the peaks that built up while the editor was closed
    audioProcessor.meterEngine.read();
    audioProcessor.truePeakDetector.read();
}

MultiMeterAudioProcessorEditor::~MultiMeterAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

//==============================================================================
void MultiMeterAudioProcessorEditor::paint(juce::Graphics& g)
{
    // The views are opaque, so this only shows in the header, the bottom bar, and the gaps between the displays
    g.fillAll(Theme::window);

    auto header = getLocalBounds().removeFromTop(Theme::headerHeight);
    g.setGradientFill(juce::ColourGradient(Theme::windowLight, 0.f, 0.f, Theme::window, 0.f, (float)header.getBottom(), false));
    g.fillRect(header);

    // The name of the plugin, with its second half in the accent color
    auto name = header.withTrimmedLeft(16);
    const auto nameFont = Theme::font(15.f, true);
    g.setFont(nameFont);
    g.setColour(Theme::text);
    g.drawText("Multi", name, juce::Justification::centredLeft);
    g.setColour(Theme::accent);
    g.drawText("Meter", name.withTrimmedLeft(Theme::textWidth(nameFont, "Multi")), juce::Justification::centredLeft);
}

void MultiMeterAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    auto header = bounds.removeFromTop(Theme::headerHeight);
    header.removeFromLeft(116);
    tabs.setBounds(header.removeFromLeft(juce::jmin(header.getWidth(), tabs.getIdealWidth())));

    auto bottomBar = bounds.removeFromBottom(Theme::bottomBarHeight);
    bottomBar.removeFromRight(18); // the corner resizer
    meterSettingsButton.setBounds(bottomBar.removeFromRight(meterSettingsButton.getIdealWidth()));
    controlBar.setBounds(bottomBar.withTrimmedLeft(8));

    // The side column, from the bottom up: the correlation, the loudness, and the bars in what is left
    bounds.removeFromTop(Theme::gap);
    bounds.removeFromBottom(Theme::gap);
    auto side = bounds.removeFromRight(Theme::sideColumnWidth);
    bounds.removeFromRight(Theme::gap);

    correlationBar.setBounds(side.removeFromBottom(58));
    side.removeFromBottom(Theme::gap);
    loudnessSummary.setBounds(side.removeFromBottom(132));
    side.removeFromBottom(Theme::gap);
    levelMeters.setBounds(side);

    for (auto* view : std::initializer_list<juce::Component*> { &goniometerView, &spectrumView, &spectrogramView, &historyView, &loudnessView })
        view->setBounds(bounds);

    // Keep the size for the next time that the editor opens
    if (isConstructed)
    {
        audioProcessor.apvts.state.setProperty(editorWidthProperty, getWidth(), nullptr);
        audioProcessor.apvts.state.setProperty(editorHeightProperty, getHeight(), nullptr);
    }
}

//==============================================================================
void MultiMeterAudioProcessorEditor::vBlank(double timestampSeconds)
{
    if (lastUpdateTime < 0.0)
    {
        lastUpdateTime = timestampSeconds;
        lastAudioTime = timestampSeconds;
        return;
    }

    // Skip the frames of the display that come sooner than the refresh rate asks for.
    // The small tolerance keeps the timing jitter of the display from dropping a frame that is due.
    const double rateHz = Parameters::valueAt(Parameters::refreshRatesHz, getChoice(Parameters::ID::refreshRate));
    const double elapsedSeconds = timestampSeconds - lastUpdateTime;
    if (elapsedSeconds < 0.9 / rateHz)
        return;

    lastUpdateTime = timestampSeconds;

    // After a long pause, such as the window being hidden, the meters carry on rather than jump
    updateMeters((float)juce::jmin(elapsedSeconds, 0.1));
}

void MultiMeterAudioProcessorEditor::updateMeters(float elapsedSeconds)
{
    using namespace Parameters;

    // The measurements were made on the audio thread from every sample
    auto readings = audioProcessor.meterEngine.read();
    auto loudness = audioProcessor.loudnessMeter.read();
    const auto truePeak = audioProcessor.truePeakDetector.read();

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
    {
        readings = {};

        // The momentary and short-term loudness describe the present, so they go quiet with the audio.
        // The integrated loudness and the range describe the programme so far, so they stay.
        loudness.momentary = loudness.shortTerm = LoudnessMeter::silence;
    }

    auto toDecibels = [](float gain) { return juce::Decibels::gainToDecibels(gain, -200.f); };
    const float target = valueAt(loudnessTargetsLufs, getChoice(ID::loudnessTarget));

    // The side column
    LevelMeters::Levels levels;
    for (size_t channel = 0; channel < 2; ++channel)
    {
        levels.peakDb[channel] = toDecibels(readings.peak[channel]);
        levels.rmsDb[channel] = toDecibels(readings.rms[channel]);
    }
    levels.momentaryLufs = loudness.momentary;
    levels.shortTermLufs = loudness.shortTerm;
    levels.integratedLufs = loudness.integrated;
    levels.targetLufs = target;

    LevelMeters::Settings meterSettings;
    meterSettings.tickDecayDbPerSecond = valueAt(decayRatesDbPerSecond, getChoice(ID::decayRate));
    meterSettings.tickHoldSeconds = valueAt(holdTimesSeconds, getChoice(ID::holdTime));
    meterSettings.showPeak = getChoice(ID::meterView) != rmsMeters;
    meterSettings.showRms = getChoice(ID::meterView) != peakMeters;
    meterSettings.showTicks = isOn(ID::showTick);
    meterSettings.resetTicks = resetTicksRequested;
    resetTicksRequested = false;

    levelMeters.update(levels, meterSettings, elapsedSeconds);
    correlationBar.update(readings.correlationFast, readings.correlationSlow);

    const float truePeakDb = audioRunning ? toDecibels(juce::jmax(truePeak.peak[0], truePeak.peak[1])) : -200.f;
    const float maxTruePeakDb = toDecibels(juce::jmax(truePeak.maxPeak[0], truePeak.maxPeak[1]));
    loudnessSummary.update(loudness, maxTruePeakDb, target, elapsedSeconds);

    // These views keep a history, so they record whichever view is showing
    loudnessView.update(loudness, truePeakDb, maxTruePeakDb, target, audioRunning, elapsedSeconds);

    if (audioRunning)
        historyView.update(juce::jmax(levels.peakDb[0], levels.peakDb[1]), juce::jmax(levels.rmsDb[0], levels.rmsDb[1]), elapsedSeconds);

    // Only the visible view needs the samples themselves
    if (goniometerView.isVisible())
    {
        const auto mode = getChoice(ID::goniometerMode) == polarMode ? GoniometerView::polar : GoniometerView::lissajous;
        const float persistence = valueAt(goniometerPersistenceSeconds, getChoice(ID::goniometerPersistence));
        goniometerView.update(audioProcessor.sampleRingBuffer, elapsedSeconds, mode, persistence, getValue(ID::goniometerScale) / 100.f);
    }

    if (spectrumView.isVisible() || spectrogramView.isVisible())
    {
        // While frozen the spectra stay as they are, but a change of setting still shows
        const bool frozen = freezeButton != nullptr && freezeButton->getToggleState();
        const int order = valueAt(spectrumResolutionOrders, getChoice(ID::spectrumResolution));
        const float tilt = valueAt(spectrumTiltsDbPerOctave, getChoice(ID::spectrumTilt));
        const bool hasNewSpectra = !frozen && spectrumSource.update(elapsedSeconds, order);

        if (spectrumView.isVisible())
        {
            SpectrumView::Settings spectrumSettings;
            spectrumSettings.midSide = getChoice(ID::spectrumChannels) == 1;
            spectrumSettings.tiltDbPerOctave = tilt;
            spectrumSettings.smoothingOctaves = valueAt(spectrumSmoothingOctaves, getChoice(ID::spectrumSmoothing));
            spectrumSettings.peakHold = isOn(ID::spectrumPeakHold);
            spectrumView.update(hasNewSpectra, spectrumSettings);
        }
        else if (hasNewSpectra)
        {
            spectrogramView.addColumn(tilt);
        }
    }
}

//==============================================================================
void MultiMeterAudioProcessorEditor::showMainView(int viewId)
{
    tabs.setSelection(viewId);
    controlBar.showView(viewId);

    goniometerView.setVisible(viewId == Parameters::viewGoniometer);
    spectrumView.setVisible(viewId == Parameters::viewSpectrum);
    spectrogramView.setVisible(viewId == Parameters::viewSpectrogram);
    historyView.setVisible(viewId == Parameters::viewHistory);
    loudnessView.setVisible(viewId == Parameters::viewLoudness);
}

void MultiMeterAudioProcessorEditor::buildMeterSettingsMenu(juce::PopupMenu& menu)
{
    using namespace Parameters;
    auto& apvts = audioProcessor.apvts;

    auto addSubMenu = [&](const juce::String& title, const juce::String& parameterID)
    {
        juce::PopupMenu subMenu;
        addChoiceItems(subMenu, *apvts.getParameter(parameterID));
        menu.addSubMenu(title, subMenu);
    };

    menu.addSectionHeader("Level meters");
    addSubMenu("Show", ID::meterView);

    menu.addSectionHeader("Peak ticks");
    menu.addItem("Show ticks", true, isOn(ID::showTick), [&apvts]
    {
        auto* parameter = apvts.getParameter(ID::showTick);
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->getValue() > 0.5f ? 0.f : 1.f);
        parameter->endChangeGesture();
    });
    addSubMenu("Hold for", ID::holdTime);
    addSubMenu("Then fall at", ID::decayRate);
    menu.addItem("Reset ticks", [this] { resetTicksRequested = true; });

    menu.addSectionHeader("Correlation");
    addSubMenu("Slow reading over", ID::averagerDuration);

    menu.addSectionHeader("Display");
    addSubMenu("Redraw at", ID::refreshRate);
}

//==============================================================================
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
