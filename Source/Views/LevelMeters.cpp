#include "LevelMeters.h"

//==============================================================================
LevelMeters::LevelMeters()
{
    setOpaque(true);
    readouts.fill(Theme::formatDb(-200.f));
}

float LevelMeters::yOf(float decibels) const
{
    return juce::jmap(juce::jlimit(minDb, maxDb, decibels), minDb, maxDb, (float)barsArea.getBottom(), (float)barsArea.getY());
}

void LevelMeters::resized()
{
    auto bounds = getLocalBounds().reduced(14, 10);

    auto readoutRow = bounds.removeFromTop(16);
    bounds.removeFromTop(6);
    bounds.removeFromBottom(16); // the names of the bars
    barsArea = bounds;

    // From the left: the two channels, the scale, and the two loudness bars. The scale has a fixed
    // width, and the bars share the rest: two parts for each channel and the short-term loudness,
    // and one for the momentary loudness.
    const float scaleWidth = 44.f, space = 3.f;
    auto row = bounds.toFloat();
    const float part = (row.getWidth() - scaleWidth - 2.f * space) / 7.f;

    channelBars[0] = row.removeFromLeft(2.f * part);
    row.removeFromLeft(space);
    channelBars[1] = row.removeFromLeft(2.f * part);

    shortTermBar = row.removeFromRight(2.f * part);
    row.removeFromRight(space);
    momentaryBar = row.removeFromRight(part);

    for (size_t channel = 0; channel < 2; ++channel)
        readoutAreas[channel] = channelBars[channel].toNearestInt().withY(readoutRow.getY()).withHeight(readoutRow.getHeight()).expanded(1, 0);

    // Blue through the working range, yellow as the level nears full scale, and red above it
    const juce::Point<float> foot(0.f, (float)barsArea.getBottom()), top(0.f, (float)barsArea.getY());
    levelGradient = Theme::levelColours(minDb, maxDb, -6.f, 0.f, foot, top);
    updateLoudnessGradient();

    staticLayer.invalidate();
}

void LevelMeters::updateLoudnessGradient()
{
    // The loudness bars turn yellow at the target, and red 6 LU above it, as the level bars do at full scale.
    // Without a target they stay blue.
    const juce::Point<float> foot(0.f, (float)barsArea.getBottom()), top(0.f, (float)barsArea.getY());
    const bool hasTarget = shown.targetLufs < 0.f;
    loudnessGradient = Theme::levelColours(minDb, maxDb, hasTarget ? shown.targetLufs : 100.f, hasTarget ? shown.targetLufs + 6.f : 200.f, foot, top);
    gradientTargetLufs = shown.targetLufs;
}

void LevelMeters::mouseDown(const juce::MouseEvent&)
{
    for (auto& channel : channels)
        channel.highest = channel.tick = -200.f;

    secondsSinceReadout = Theme::readoutIntervalSeconds;
}

void LevelMeters::update(const Levels& levels, const Settings& newSettings, float elapsedSeconds)
{
    // Remember where everything was drawn, to repaint only if a pixel has moved
    auto positions = [this]
    {
        std::array<int, 9> result {};
        for (size_t i = 0; i < 2; ++i)
        {
            result[i * 3] = juce::roundToInt(yOf(channels[i].peak));
            result[i * 3 + 1] = juce::roundToInt(yOf(channels[i].rms));
            result[i * 3 + 2] = juce::roundToInt(yOf(channels[i].tick));
        }
        result[6] = juce::roundToInt(yOf(shown.momentaryLufs));
        result[7] = juce::roundToInt(yOf(shown.shortTermLufs));
        result[8] = juce::roundToInt(yOf(shown.integratedLufs) + yOf(shown.targetLufs));
        return result;
    };

    const auto before = positions();
    bool needsRepaint = newSettings.showPeak != settings.showPeak || newSettings.showRms != settings.showRms || newSettings.showTicks != settings.showTicks;
    settings = newSettings;

    for (size_t i = 0; i < 2; ++i)
    {
        auto& channel = channels[i];

        // The peak bar rises at once and falls at a steady rate
        channel.peak = juce::jmax(levels.peakDb[i], channel.peak - peakReleaseDbPerSecond * elapsedSeconds, -200.f);
        channel.rms = levels.rmsDb[i];
        channel.highest = juce::jmax(channel.highest, levels.peakDb[i]);

        // The tick holds the highest peak, then falls faster and faster
        channel.secondsSinceTick += elapsedSeconds;
        if (settings.resetTicks)
            channel.tick = -200.f;

        if (levels.peakDb[i] >= channel.tick)
        {
            channel.tick = levels.peakDb[i];
            channel.secondsSinceTick = 0.f;
            channel.tickDecayMultiplier = 1.f;
        }
        else if (channel.secondsSinceTick > settings.tickHoldSeconds)
        {
            channel.tick = juce::jmax(-200.f, channel.tick - settings.tickDecayDbPerSecond * elapsedSeconds * channel.tickDecayMultiplier);
            channel.tickDecayMultiplier *= std::pow(1.05f, elapsedSeconds * 60.f);
        }
    }

    shown = levels;

    if (!juce::exactlyEqual(shown.targetLufs, gradientTargetLufs))
        updateLoudnessGradient();

    // The numbers change ten times a second, which is as fast as they can be read
    secondsSinceReadout += (double)elapsedSeconds;
    if (secondsSinceReadout >= Theme::readoutIntervalSeconds)
    {
        secondsSinceReadout = 0.0;
        for (size_t i = 0; i < 2; ++i)
        {
            const auto text = Theme::formatDb(channels[i].highest, minDb - 40.f);
            if (text != readouts[i])
            {
                readouts[i] = text;
                repaint(readoutAreas[i]);
            }
        }
    }

    if (needsRepaint || positions() != before)
        repaint(barsArea.expanded(2, 2));
}

void LevelMeters::paint(juce::Graphics& g)
{
    staticLayer.draw(g, getLocalBounds(), true, [this](juce::Graphics& layer) { paintStaticLayer(layer); });

    for (size_t i = 0; i < 2; ++i)
    {
        paintChannel(g, channels[i], channelBars[i]);

        g.setFont(Theme::font(11.f));
        g.setColour(channels[i].highest > 0.f ? Theme::over : Theme::text);
        g.drawText(readouts[i], readoutAreas[i], juce::Justification::centred);
    }

    // The loudness bars: a body that is held back, under a bright line at the reading
    for (const auto& [bar, lufs] : { std::pair { momentaryBar, shown.momentaryLufs }, std::pair { shortTermBar, shown.shortTermLufs } })
    {
        if (lufs <= minDb)
            continue;

        const float y = yOf(lufs);
        g.setGradientFill(loudnessGradient);
        g.setOpacity(Theme::barBodyAlpha);
        g.fillRect(bar.withTop(y));
        g.setOpacity(1.f);

        g.setColour(capColourAt(loudnessGradient, y));
        g.fillRect(bar.withTop(y).withHeight(Theme::barCapHeight));
    }

    const auto loudnessSpan = momentaryBar.getUnion(shortTermBar);

    if (shown.targetLufs < 0.f)
    {
        // A notch on either side of the bars marks the target
        const float y = yOf(shown.targetLufs);
        g.setColour(Theme::target);
        g.fillRect(loudnessSpan.getX() - 5.f, y - 0.5f, 4.f, 1.5f);
        g.fillRect(loudnessSpan.getRight() + 1.f, y - 0.5f, 4.f, 1.5f);
    }

    if (shown.integratedLufs > minDb)
    {
        g.setColour(juce::Colours::white);
        g.fillRect(loudnessSpan.getX(), yOf(shown.integratedLufs) - 0.5f, loudnessSpan.getWidth(), 1.5f);
    }
}

void LevelMeters::paintChannel(juce::Graphics& g, const Channel& channel, juce::Rectangle<float> bar) const
{
    const float rmsY = settings.showRms ? yOf(channel.rms) : bar.getBottom();

    if (settings.showRms && channel.rms > minDb)
    {
        g.setGradientFill(levelGradient);
        g.setOpacity(Theme::barBodyAlpha);
        g.fillRect(bar.withTop(rmsY));
        g.setOpacity(1.f);

        g.setColour(capColourAt(levelGradient, rmsY));
        g.fillRect(bar.withTop(rmsY).withHeight(Theme::barCapHeight));
    }

    if (settings.showPeak && channel.peak > minDb)
    {
        // Beside the RMS bar the peak is only the lighter part above it
        const float peakY = yOf(channel.peak);
        if (peakY < rmsY)
        {
            g.setGradientFill(levelGradient);
            g.setOpacity(settings.showRms ? 0.38f : 1.f);
            g.fillRect(bar.withTop(peakY).withBottom(rmsY));
            g.setOpacity(1.f);
        }
    }

    if (settings.showTicks && channel.tick > minDb)
    {
        g.setColour(channel.tick > 0.f ? Theme::over : Theme::held);
        g.fillRect(bar.getX(), yOf(channel.tick) - 1.f, bar.getWidth(), 2.f);
    }
}

void LevelMeters::paintStaticLayer(juce::Graphics& g)
{
    Theme::fillDisplay(g, getLocalBounds());

    // The unlit bars
    g.setColour(Theme::track);
    for (auto& bar : channelBars)
        g.fillRect(bar);
    g.fillRect(momentaryBar);
    g.fillRect(shortTermBar);

    // The scale runs between the channels and the loudness bars
    const auto scaleArea = juce::Rectangle<float>(channelBars[1].getRight(), (float)barsArea.getY(), momentaryBar.getX() - channelBars[1].getRight(), (float)barsArea.getHeight());

    g.setFont(Theme::font(10.5f));
    for (int decibels = 0; decibels >= (int)minDb; decibels -= 6)
    {
        const float y = yOf((float)decibels);

        // The bottom of the scale is left for the unit
        const bool labelled = decibels % 12 == 0 && decibels > (int)minDb;

        g.setColour(labelled ? Theme::gridStrong : Theme::grid);
        g.fillRect(scaleArea.getX() + 4.f, y, labelled ? 5.f : 3.f, 1.f);
        g.fillRect(scaleArea.getRight() - (labelled ? 9.f : 7.f), y, labelled ? 5.f : 3.f, 1.f);

        if (labelled)
        {
            g.setColour(decibels == 0 ? Theme::textDim : Theme::textFaint);
            g.drawText(juce::String(std::abs(decibels)), scaleArea.withY(y - 7.f).withHeight(14.f), juce::Justification::centred);
        }
    }

    // A line across the channels at full scale
    g.setColour(Theme::over.withAlpha(0.35f));
    for (auto& bar : channelBars)
        g.fillRect(bar.getX(), yOf(0.f), bar.getWidth(), 1.f);

    // The names of the bars
    const auto names = getLocalBounds().withY(barsArea.getBottom() + 3).withHeight(14);
    g.setColour(Theme::textDim);
    g.drawText("L", channelBars[0].toNearestInt().withY(names.getY()).withHeight(14), juce::Justification::centred);
    g.drawText("R", channelBars[1].toNearestInt().withY(names.getY()).withHeight(14), juce::Justification::centred);
    g.drawText("M", momentaryBar.toNearestInt().withY(names.getY()).withHeight(14), juce::Justification::centred);
    g.drawText("S", shortTermBar.toNearestInt().withY(names.getY()).withHeight(14), juce::Justification::centred);
    g.setColour(Theme::textFaint);
    g.drawText("dB", scaleArea.toNearestInt().withY(names.getY()).withHeight(14), juce::Justification::centred);
}
