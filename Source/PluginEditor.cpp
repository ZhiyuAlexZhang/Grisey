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
GriseyAudioProcessorEditor::GriseyAudioProcessorEditor(GriseyAudioProcessor& p) :
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

    controlBar.addMenu(ControlBar::views({ viewGoniometer }), apvts, ID::goniometerMode, "Mode:");
    controlBar.addMenu(ControlBar::views({ viewGoniometer }), apvts, ID::goniometerPersistence, "Persistence:");

    controlBar.addMenu(ControlBar::views({ viewSpectrum }), apvts, ID::spectrumChannels, "Channels:");
    controlBar.addMenu(ControlBar::views({ viewSpectrum, viewSpectrogram }), apvts, ID::spectrumTilt, "Tilt:");
    controlBar.addMenu(ControlBar::views({ viewSpectrum }), apvts, ID::spectrumSmoothing, "Smoothing:");
    controlBar.addMenu(ControlBar::views({ viewSpectrum, viewSpectrogram }), apvts, ID::spectrumResolution, "FFT:");
    controlBar.addToggle(ControlBar::views({ viewSpectrum }), apvts, ID::spectrumPeakHold, "Peak hold");

    // Freezing is for a moment's look, so it is not a setting that is saved
    freezeButton = &controlBar.addButton(ControlBar::views({ viewSpectrum, viewSpectrogram }), "Freeze", true, [] {});

    // The views that show time share one timeline, so they share its span
    controlBar.addMenu(ControlBar::views({ viewSpectrogram, viewHistory, viewLoudness }), apvts, ID::timeSpan, "Time span:");

    controlBar.addMenu(ControlBar::views({ viewLoudness }), apvts, ID::loudnessTarget, "Target:");
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

GriseyAudioProcessorEditor::~GriseyAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

//==============================================================================
void GriseyAudioProcessorEditor::paint(juce::Graphics& g)
{
    // The views are opaque, so this only shows as the hairlines between the displays, and under the chrome
    g.fillAll(Theme::edge);

    auto bounds = getLocalBounds();
    paintHeader(g, bounds.removeFromTop(Theme::headerHeight));
    paintBottomBar(g, bounds.removeFromBottom(Theme::bottomBarHeight));
}

namespace
{
    // The widths of the raised tab that holds the name, and of the shoulders on either side of it
    constexpr float nameTabStart = 6.f;
    constexpr float nameTabWidth = 132.f;
    constexpr float shoulderWidth = 46.f;

    // The thickness of the raised edge that runs along the rest of the chrome
    constexpr float rimThickness = 4.f;

    // Adds an S-shaped shoulder to a path, from where the path is to a point
    void shoulderTo(juce::Path& path, juce::Point<float> to)
    {
        const auto from = path.getCurrentPosition();
        const float middle = 0.5f * (from.x + to.x);
        path.cubicTo(middle, from.y, middle, to.y, to.x, to.y);
    }
}

void GriseyAudioProcessorEditor::paintHeader(juce::Graphics& g, juce::Rectangle<int> area)
{
    const auto bounds = area.toFloat();

    // The recessed strip, which holds the tabs
    g.setGradientFill(juce::ColourGradient(Theme::chromeInsetTop, 0.f, bounds.getY(), Theme::chromeInsetBottom, 0.f, bounds.getBottom(), false));
    g.fillRect(bounds);

    // The raised part: a rim along the top, which drops into a tab for the name
    const float rim = bounds.getY() + rimThickness;
    const float tabLeft = bounds.getX() + nameTabStart + shoulderWidth;
    const float tabRight = tabLeft + nameTabWidth;

    juce::Path raised;
    raised.startNewSubPath(bounds.getTopLeft());
    raised.lineTo(bounds.getTopRight());
    raised.lineTo(bounds.getRight(), rim);
    raised.lineTo(tabRight + shoulderWidth, rim);
    shoulderTo(raised, { tabRight, bounds.getBottom() });
    raised.lineTo(tabLeft, bounds.getBottom());
    shoulderTo(raised, { bounds.getX() + nameTabStart, rim });
    raised.lineTo(bounds.getX(), rim);
    raised.closeSubPath();

    g.setGradientFill(juce::ColourGradient(Theme::chromeTop, 0.f, bounds.getY(), Theme::chromeBottom, 0.f, bounds.getBottom(), false));
    g.fillPath(raised);

    // A light edge where the raised part ends, which is what makes it look raised
    g.setColour(Theme::panelEdge.withAlpha(0.7f));
    g.strokePath(raised, juce::PathStrokeType(1.f));
    g.setColour(Theme::edge);
    g.fillRect(bounds.withTop(bounds.getBottom() - 1.f));

    // The name of the plugin
    g.setFont(Theme::font(16.f));
    g.setColour(Theme::wordmark);
    g.drawText("Grisey", area.withX(juce::roundToInt(tabLeft)).withWidth(juce::roundToInt(nameTabWidth)).translated(0, 1), juce::Justification::centred);
}

void GriseyAudioProcessorEditor::paintBottomBar(juce::Graphics& g, juce::Rectangle<int> area)
{
    const auto bounds = area.toFloat();

    // What shows where the bar dips away is the bottom of the displays
    g.setColour(Theme::displayBottom);
    g.fillRect(bounds);

    // The bar is raised along its whole length, except for a dip between the controls of the view
    // and the settings of the meters, if the window is wide enough to leave room for one
    const float dipLeft = (float)controlBar.getX() + (float)controlBar.getUsedWidth() + 24.f;
    const float dipRight = (float)meterSettingsButton.getX() - 12.f;
    const bool hasDip = dipRight - dipLeft > 2.f * shoulderWidth + 20.f;
    const float rim = bounds.getBottom() - rimThickness;

    juce::Path raised;
    raised.startNewSubPath(bounds.getTopLeft());

    if (hasDip)
    {
        raised.lineTo(dipLeft, bounds.getY());
        shoulderTo(raised, { dipLeft + shoulderWidth, rim });
        raised.lineTo(dipRight - shoulderWidth, rim);
        shoulderTo(raised, { dipRight, bounds.getY() });
    }

    raised.lineTo(bounds.getTopRight());
    raised.lineTo(bounds.getBottomRight());
    raised.lineTo(bounds.getBottomLeft());
    raised.closeSubPath();

    g.setGradientFill(juce::ColourGradient(Theme::footerTop.brighter(0.08f), 0.f, bounds.getY(), Theme::footerBottom, 0.f, bounds.getBottom(), false));
    g.fillPath(raised);
    g.setColour(Theme::panelEdge.withAlpha(0.7f));
    g.strokePath(raised, juce::PathStrokeType(1.f));
}

void GriseyAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // The tabs start where the shoulder of the name's tab has come up to the rim
    auto header = bounds.removeFromTop(Theme::headerHeight);
    header.removeFromLeft(juce::roundToInt(nameTabStart + nameTabWidth + 2.f * shoulderWidth) + 4);
    header.removeFromTop(juce::roundToInt(rimThickness));
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
void GriseyAudioProcessorEditor::vBlank(double timestampSeconds)
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

void GriseyAudioProcessorEditor::updateMeters(float elapsedSeconds)
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

    // The spectrogram, the history and the loudness share one timeline. They all record in every
    // update, whichever view is showing, so that the same moment is in the same place in all of them.
    // While no audio arrives the clock stands still, and so do all three.
    const int numNewSlots = timelineClock.advance(elapsedSeconds, audioRunning);
    const float timeSpan = valueAt(timeSpansSeconds, getChoice(ID::timeSpan));

    loudnessView.setSpan(timeSpan);
    historyView.setSpan(timeSpan);
    spectrogramView.setSpan(timeSpan);

    loudnessView.update(loudness, truePeakDb, maxTruePeakDb, target, numNewSlots, elapsedSeconds);
    historyView.record(numNewSlots, juce::jmax(levels.peakDb[0], levels.peakDb[1]), juce::jmax(levels.rmsDb[0], levels.rmsDb[1]));

    // Only the visible view needs the samples themselves
    if (goniometerView.isVisible())
    {
        const auto mode = getChoice(ID::goniometerMode) == polarMode ? GoniometerView::polar : GoniometerView::lissajous;
        const float persistence = valueAt(goniometerPersistenceSeconds, getChoice(ID::goniometerPersistence));
        goniometerView.update(audioProcessor.sampleRingBuffer, elapsedSeconds, mode, persistence, getValue(ID::goniometerScale) / 100.f);
    }

    // The spectra are analyzed in every update, because the spectrogram records them whether it is
    // showing or not. Two FFTs cost very little beside drawing a frame.
    {
        const int order = valueAt(spectrumResolutionOrders, getChoice(ID::spectrumResolution));
        const float tilt = valueAt(spectrumTiltsDbPerOctave, getChoice(ID::spectrumTilt));
        const bool hasNewSpectra = spectrumSource.update(elapsedSeconds, order);

        // While frozen the pictures stand still, but the recording carries on underneath
        const bool frozen = freezeButton != nullptr && freezeButton->getToggleState();
        spectrogramView.record(numNewSlots, hasNewSpectra, tilt, frozen);

        if (spectrumView.isVisible())
        {
            SpectrumView::Settings spectrumSettings;
            spectrumSettings.midSide = getChoice(ID::spectrumChannels) == 1;
            spectrumSettings.tiltDbPerOctave = tilt;
            spectrumSettings.smoothingOctaves = valueAt(spectrumSmoothingOctaves, getChoice(ID::spectrumSmoothing));
            spectrumSettings.peakHold = isOn(ID::spectrumPeakHold);
            spectrumView.update(hasNewSpectra, spectrumSettings, elapsedSeconds, frozen);
        }
    }
}

//==============================================================================
void GriseyAudioProcessorEditor::showMainView(int viewId)
{
    tabs.setSelection(viewId);
    controlBar.showView(viewId);

    // The dip in the bottom bar follows the width of the view's controls
    repaint(getLocalBounds().removeFromBottom(Theme::bottomBarHeight));

    goniometerView.setVisible(viewId == Parameters::viewGoniometer);
    spectrumView.setVisible(viewId == Parameters::viewSpectrum);
    spectrogramView.setVisible(viewId == Parameters::viewSpectrogram);
    historyView.setVisible(viewId == Parameters::viewHistory);
    loudnessView.setVisible(viewId == Parameters::viewLoudness);
}

void GriseyAudioProcessorEditor::buildMeterSettingsMenu(juce::PopupMenu& menu)
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
float GriseyAudioProcessorEditor::getValue(const juce::String& parameterID) const
{
    return audioProcessor.apvts.getRawParameterValue(parameterID)->load();
}

int GriseyAudioProcessorEditor::getChoice(const juce::String& parameterID) const
{
    return juce::roundToInt(getValue(parameterID));
}

bool GriseyAudioProcessorEditor::isOn(const juce::String& parameterID) const
{
    return getValue(parameterID) > 0.5f;
}
