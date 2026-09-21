#include "HistoryView.h"

//==============================================================================
float HistoryView::yOf(float decibels) const
{
    return juce::jmap(juce::jlimit(minDb, maxDb, decibels), minDb, maxDb, (float)plot.getBottom(), (float)plot.getY());
}

void HistoryView::resized()
{
    // The same margins as the spectrogram, so that a moment is at the same place in both
    plot = getLocalBounds().withTrimmedLeft(34).withTrimmedRight(34).withTrimmedTop(8).withTrimmedBottom(20);

    // The same colors as the level meters: blue through the working range, yellow near full scale, red above it
    auto proportionOf = [](float decibels) { return (double)juce::jmap(decibels, minDb, maxDb, 0.f, 1.f); };

    juce::ColourGradient gradient(Theme::secondDeep, 0.f, 0.f, Theme::over, 0.f, 1.f, false);
    gradient.addColour(proportionOf(-24.f), Theme::second);
    gradient.addColour(proportionOf(-6.f), Theme::accent);
    gradient.addColour(proportionOf(0.f), Theme::over);

    // The background of each row is what Theme::fillDisplay() draws at its height
    const juce::ColourGradient background(Theme::displayTop, 0.f, 0.f, Theme::displayBottom, 0.f, 1.f, false);

    const int height = juce::jmax(1, plot.getHeight());
    rmsColours.resize((size_t)height);
    peakColours.resize((size_t)height);
    edgeColours.resize((size_t)height);
    backgroundColours.resize((size_t)height);

    for (int row = 0; row < height; ++row)
    {
        const double y = (double)(plot.getY() + row);
        const auto behind = background.getColourAtPosition(juce::jlimit(0.0, 1.0, (y - 0.25 * getHeight()) / (0.75 * getHeight())));
        const auto colour = gradient.getColourAtPosition(1.0 - (double)row / (double)height);

        // A shape is strongest at the top of the plot and fades on the way down, so that the bottom of the
        // display stays dark, and its upper edge is a line of the full color
        const float depth = (float)row / (float)height;

        backgroundColours[(size_t)row] = behind;
        rmsColours[(size_t)row] = behind.interpolatedWith(colour, juce::jmap(depth, 0.62f, 0.16f));
        peakColours[(size_t)row] = behind.interpolatedWith(colour, juce::jmap(depth, 0.26f, 0.08f));
        edgeColours[(size_t)row] = colour.brighter(0.15f);
    }

    rebuildImage();
}

void HistoryView::setSpan(float seconds)
{
    if (!juce::exactlyEqual(seconds, spanSeconds))
    {
        spanSeconds = seconds;
        rebuildImage();
        repaint();
    }
}

void HistoryView::record(int numNewSlots, float peakDb, float rmsDb)
{
    pending.peak = juce::jmax(pending.peak, peakDb);
    pending.rms = juce::jmax(pending.rms, rmsDb);

    if (numNewSlots <= 0)
        return;

    // A frame that took long completes more than one slot, which all show the same levels
    for (int slot = 0; slot < numNewSlots; ++slot)
    {
        history.push(pending);
        addColumn(pending);
    }

    pending = {};
    repaint(plot);
}

void HistoryView::addColumn(const Levels& levels)
{
    if (rmsColours.empty())
        return;

    const int peakRow = juce::roundToInt(yOf(levels.peak)) - plot.getY();
    const int rmsRow = juce::roundToInt(yOf(levels.rms)) - plot.getY();
    const bool hasPeak = showPeak && levels.peak > minDb, hasRms = showRms && levels.rms > minDb;
    const int lastRow = (int)rmsColours.size() - 1;
    const auto& peakShape = showRms ? peakColours : rmsColours;

    image.addColumn([&](int row)
    {
        const auto index = (size_t)juce::jmin(row, lastRow);
        // The upper edge of the shape in front is a line two rows high
        const int edgeRow = hasRms ? rmsRow : peakRow;
        if ((hasRms || hasPeak) && row >= edgeRow && row < edgeRow + 2)
            return edgeColours[index];

        return hasRms && row >= rmsRow ? rmsColours[index]
             : hasPeak && row >= peakRow ? peakShape[index]
             : backgroundColours[index];
    });
}

void HistoryView::rebuildImage()
{
    if (plot.isEmpty())
        return;

    // One column for every slot of the span, from the oldest to the newest. A slot that
    // nothing was recorded in is background.
    const int numSlots = Timeline::slotsIn(spanSeconds);
    image.setSize(numSlots, plot.getHeight(), Theme::display);

    for (int age = numSlots - 1; age >= 0; --age)
    {
        const auto* levels = history.fromNewest(age);
        addColumn(levels != nullptr ? *levels : Levels());
    }
}

void HistoryView::paint(juce::Graphics& g)
{
    Theme::fillDisplay(g, getLocalBounds());
    image.draw(g, plot);

    // A line for every 12 dB, stronger at 0, drawn over the image so that it scrolls underneath
    g.setFont(Theme::font(10.5f));
    for (int decibels = 0; decibels > (int)minDb; decibels -= 12)
    {
        const float y = yOf((float)decibels);
        g.setColour(juce::Colours::white.withAlpha(decibels == 0 ? 0.16f : 0.07f));
        g.fillRect((float)plot.getX(), y, (float)plot.getWidth(), 1.f);

        g.setColour(Theme::accent.withAlpha(decibels == 0 ? 0.9f : 0.55f));
        g.drawText(juce::String(decibels), juce::Rectangle<float>((float)plot.getRight() + 4.f, y - 7.f, 28.f, 14.f), juce::Justification::centredLeft);
    }

    Timeline::drawTimeAxis(g, plot, spanSeconds);

    // Name the shapes that are showing, in their colors
    auto legend = plot.withTrimmedLeft(10).withTrimmedTop(6).removeFromTop(14);
    g.setFont(Theme::labelFont());

    if (showRms)
    {
        g.setColour(Theme::second);
        g.drawText("RMS", legend.removeFromLeft(34), juce::Justification::centredLeft);
    }

    if (showPeak)
    {
        g.setColour(Theme::second.withAlpha(showRms ? 0.5f : 1.f));
        g.drawText("PEAK", legend.removeFromLeft(40), juce::Justification::centredLeft);
    }
}

void HistoryView::setShown(bool peak, bool rms)
{
    if (peak == showPeak && rms == showRms)
        return;

    showPeak = peak;
    showRms = rms;
    rebuildImage();
    repaint();
}

void HistoryView::clearHistory()
{
    history.clear();
    rebuildImage();
    repaint();
}
