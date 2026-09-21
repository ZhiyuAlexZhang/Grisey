#include "HistoryView.h"

//==============================================================================
float HistoryView::yOf(float decibels) const
{
    return juce::jmap(juce::jlimit(minDb, maxDb, decibels), minDb, maxDb, (float)plot.getBottom(), (float)plot.getY());
}

void HistoryView::resized()
{
    plot = getLocalBounds().withTrimmedRight(34).withTrimmedBottom(20).withTrimmedTop(8).withTrimmedLeft(8);

    // The same colors as the level meters: blue through the working range, yellow near full scale, red above it
    auto proportionOf = [](float decibels) { return (double)juce::jmap(decibels, minDb, maxDb, 0.f, 1.f); };

    juce::ColourGradient gradient(Theme::secondDeep, 0.f, 0.f, Theme::over, 0.f, 1.f, false);
    gradient.addColour(proportionOf(-24.f), Theme::second);
    gradient.addColour(proportionOf(-6.f), Theme::accent);
    gradient.addColour(proportionOf(0.f), Theme::over);

    // The background of each row is what Theme::fillDisplay() draws at its height
    const juce::ColourGradient background(Theme::displayTop, 0.f, 0.25f * (float)getHeight(), Theme::displayBottom, 0.f, (float)getHeight(), false);

    const int height = juce::jmax(1, plot.getHeight());
    rmsColours.resize((size_t)height);
    peakColours.resize((size_t)height);
    backgroundColours.resize((size_t)height);

    for (int row = 0; row < height; ++row)
    {
        const double y = (double)(plot.getY() + row);
        const auto behind = background.getColourAtPosition(juce::jlimit(0.0, 1.0, (y - 0.25 * getHeight()) / (0.75 * getHeight())));
        const auto colour = gradient.getColourAtPosition(1.0 - (double)row / (double)height);

        backgroundColours[(size_t)row] = behind;
        rmsColours[(size_t)row] = behind.interpolatedWith(colour, 0.85f);
        peakColours[(size_t)row] = behind.interpolatedWith(colour, 0.3f);
    }

    image.setSize(plot.getWidth(), plot.getHeight(), Theme::display);
    clearImage();
}

void HistoryView::clearImage()
{
    // Every column starts as background
    for (int column = 0; column < image.getWidth(); ++column)
        image.addColumn([this](int row) { return backgroundColours[(size_t)row]; });
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
             : backgroundColours[(size_t)row];
    });

    columnPeak = columnRms = -200.f;
    repaint(plot);
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
    g.setFont(Theme::labelFont());
    g.setColour(Theme::second);
    g.drawText("RMS", legend.removeFromLeft(34), juce::Justification::centredLeft);
    g.setColour(Theme::second.withAlpha(0.5f));
    g.drawText("PEAK", legend.removeFromLeft(40), juce::Justification::centredLeft);
}

void HistoryView::mouseDown(const juce::MouseEvent&)
{
    clearImage();
    repaint();
}
