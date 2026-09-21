#include "HistoryView.h"

//==============================================================================
float HistoryView::yOf(float decibels) const
{
    return juce::jmap(juce::jlimit(minDb, maxDb, decibels), minDb, maxDb, (float)plot.getBottom(), (float)plot.getY());
}

void HistoryView::resized()
{
    plot = getLocalBounds().withTrimmedRight(34).withTrimmedBottom(20).withTrimmedTop(8).withTrimmedLeft(8);
    image.setSize(plot.getWidth(), plot.getHeight(), Theme::display);

    // The same colors as the level meters: blue through the working range, warm near full scale, red above it
    auto proportionOf = [](float decibels) { return (double)juce::jmap(decibels, minDb, maxDb, 0.f, 1.f); };

    juce::ColourGradient gradient(Theme::accent.darker(0.7f), 0.f, 0.f, Theme::over, 0.f, 1.f, false);
    gradient.addColour(proportionOf(-18.f), Theme::accent);
    gradient.addColour(proportionOf(-6.f), Theme::held);
    gradient.addColour(proportionOf(0.f), Theme::over);

    const int height = juce::jmax(1, plot.getHeight());
    rmsColours.resize((size_t)height);
    peakColours.resize((size_t)height);

    for (int row = 0; row < height; ++row)
    {
        const auto colour = gradient.getColourAtPosition(1.0 - (double)row / (double)height);
        rmsColours[(size_t)row] = Theme::display.interpolatedWith(colour, 0.85f);
        peakColours[(size_t)row] = Theme::display.interpolatedWith(colour, 0.3f);
    }
}

void HistoryView::update(float peakDb, float rmsDb, float elapsedSeconds)
{
    columnPeak = juce::jmax(columnPeak, peakDb);
    columnRms = juce::jmax(columnRms, rmsDb);
    secondsInColumn += (double)elapsedSeconds;

    const double secondsPerColumn = windowSeconds / (double)juce::jmax(1, image.getWidth());
    if (secondsInColumn < secondsPerColumn)
        return;

    secondsInColumn = std::fmod(secondsInColumn, secondsPerColumn);

    const int peakRow = juce::roundToInt(yOf(columnPeak)) - plot.getY();
    const int rmsRow = juce::roundToInt(yOf(columnRms)) - plot.getY();
    const bool hasPeak = columnPeak > minDb, hasRms = columnRms > minDb;

    image.addColumn([&](int row)
    {
        return hasRms && row >= rmsRow ? rmsColours[(size_t)row]
             : hasPeak && row >= peakRow ? peakColours[(size_t)row]
             : Theme::display;
    });

    columnPeak = columnRms = -200.f;
    repaint(plot);
}

void HistoryView::paint(juce::Graphics& g)
{
    g.fillAll(Theme::display);
    image.draw(g, plot);

    // A line for every 12 dB, stronger at 0, drawn over the image so that it scrolls underneath
    g.setFont(Theme::labelFont());
    for (int decibels = 0; decibels > (int)minDb; decibels -= 12)
    {
        const float y = yOf((float)decibels);
        g.setColour(juce::Colours::white.withAlpha(decibels == 0 ? 0.16f : 0.07f));
        g.fillRect((float)plot.getX(), y, (float)plot.getWidth(), 1.f);

        g.setColour(decibels == 0 ? Theme::textDim : Theme::textFaint);
        g.drawText(juce::String(decibels), juce::Rectangle<float>((float)plot.getRight() + 4.f, y - 7.f, 28.f, 14.f), juce::Justification::centredLeft);
    }

    // Mark the time along the bottom
    for (int seconds = 0; seconds <= (int)windowSeconds; seconds += 5)
    {
        const float x = (float)plot.getRight() - (float)plot.getWidth() * (float)seconds / (float)windowSeconds;
        g.setColour(juce::Colours::white.withAlpha(0.07f));
        g.fillRect(x, (float)plot.getY(), 1.f, (float)plot.getHeight());

        g.setColour(Theme::textFaint);
        const auto text = seconds == 0 ? juce::String("now") : juce::String(juce::CharPointer_UTF8("\xe2\x88\x92")) + juce::String(seconds) + " s";
        g.drawText(text, juce::Rectangle<float>(50.f, 14.f).withCentre({ juce::jmin(x, (float)plot.getRight() - 14.f), (float)plot.getBottom() + 10.f }), juce::Justification::centred);
    }

    auto legend = plot.withTrimmedLeft(10).withTrimmedTop(6).removeFromTop(14);
    g.setFont(Theme::font(11.5f, true));
    g.setColour(Theme::accent);
    g.drawText("RMS", legend.removeFromLeft(34), juce::Justification::centredLeft);
    g.setColour(Theme::accent.withAlpha(0.5f));
    g.drawText("PEAK", legend.removeFromLeft(40), juce::Justification::centredLeft);
}

void HistoryView::mouseDown(const juce::MouseEvent&)
{
    image.clear(Theme::display);
    repaint();
}
