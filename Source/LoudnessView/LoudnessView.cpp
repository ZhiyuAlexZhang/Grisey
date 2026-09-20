#include "LoudnessView.h"

namespace
{
    // Formats a reading with one decimal place, or as a dash when there is none
    juce::String format(float value, bool valid)
    {
        return valid ? juce::String(value, 1) : juce::String(juce::CharPointer_UTF8("\xe2\x80\x93"));
    }

    bool isLoudness(float value)
    {
        return std::isfinite(value);
    }
}

//==============================================================================
// Implementation for the LoudnessView class
LoudnessView::LoudnessView()
{
    recentTruePeaks.fill(NEGATIVE_INFINITY);
}

void LoudnessView::paint(juce::Graphics& g)
{
    // Fill the background with a rounded rectangle using the base color
    g.setColour(BASE_COLOR);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 4);

    auto area = getLocalBounds().reduced(10);
    paintReadouts(g, area.removeFromLeft(170));
    area.removeFromLeft(10);
    paintHistory(g, area);
}

void LoudnessView::update(const LoudnessMeter::Readings& newReadings, float truePeakDb, float maxTruePeakDb,
                          float targetLufs, bool audioRunning, float elapsedSeconds)
{
    readings = newReadings;
    maxTruePeak = maxTruePeakDb;
    target = targetLufs;

    // Keep the true peaks of the last 3 s, moving on to the next slot every 100 ms
    secondsInTruePeakSlot += (double)elapsedSeconds;
    while (secondsInTruePeakSlot >= truePeakSlotSeconds)
    {
        secondsInTruePeakSlot -= truePeakSlotSeconds;
        recentTruePeakIndex = (recentTruePeakIndex + 1) % recentTruePeaks.size();
        recentTruePeaks[recentTruePeakIndex] = NEGATIVE_INFINITY;
    }

    recentTruePeaks[recentTruePeakIndex] = juce::jmax(recentTruePeaks[recentTruePeakIndex], truePeakDb);
    recentTruePeak = *std::max_element(recentTruePeaks.begin(), recentTruePeaks.end());

    if (audioRunning)
    {
        secondsSinceHistoryPoint += (double)elapsedSeconds;

        while (secondsSinceHistoryPoint >= historyIntervalSeconds)
        {
            secondsSinceHistoryPoint -= historyIntervalSeconds;
            history[(size_t)historyIndex] = { readings.shortTerm, readings.momentary };
            historyIndex = (historyIndex + 1) % historyLength;
        }
    }

    repaint();
}

void LoudnessView::clearHistory()
{
    history.fill({});
    historyIndex = 0;
    secondsSinceHistoryPoint = 0.0;
    repaint();
}

float LoudnessView::getReferenceLufs() const
{
    // Without a target the scale sits where it would for EBU R 128
    return target < 0.f ? target : -23.f;
}

void LoudnessView::paintReadouts(juce::Graphics& g, juce::Rectangle<int> area)
{
    const bool hasIntegrated = isLoudness(readings.integrated);
    const bool hasShortTerm = isLoudness(readings.shortTerm);
    const bool hasTruePeak = maxTruePeak > NEGATIVE_INFINITY;

    // The integrated loudness is the reading that a delivery is judged by, so it is the largest
    auto top = area.removeFromTop(64);
    g.setColour(juce::Colours::white.withAlpha(0.6f));
    g.setFont(12.f);
    g.drawText("INTEGRATED", top.removeFromTop(16), juce::Justification::centredLeft);

    g.setColour(HIGHLIGHT_COLOR);
    g.setFont(34.f);
    auto number = top.removeFromTop(36);
    g.drawText(format(readings.integrated, hasIntegrated), number, juce::Justification::centredLeft);

    g.setFont(12.f);
    g.setColour(juce::Colours::white.withAlpha(0.6f));
    juce::String unit = "LUFS";
    if (hasIntegrated && target < 0.f)
    {
        const float difference = readings.integrated - target;
        unit << "   " << (difference >= 0.f ? "+" : "") << juce::String(difference, 1) << " LU to target";
    }
    g.drawText(unit, top, juce::Justification::centredLeft);

    // The other readings share a table
    struct Row
    {
        const char* name;
        juce::String value;
        const char* unit;
        bool warn;
    };

    const Row rows[] {
        { "SHORT TERM", format(readings.shortTerm, hasShortTerm), "LUFS", false },
        { "MOMENTARY", format(readings.momentary, isLoudness(readings.momentary)), "LUFS", false },
        { "RANGE", format(readings.range, isLoudness(readings.rangeLow)), "LU", false },
        { "TRUE PEAK", format(maxTruePeak, hasTruePeak), "dBTP", hasTruePeak && maxTruePeak > truePeakLimitDb },
        { "PLR", format(maxTruePeak - readings.integrated, hasTruePeak && hasIntegrated), "LU", false },
        { "PSR", format(recentTruePeak - readings.shortTerm, recentTruePeak > NEGATIVE_INFINITY && hasShortTerm), "LU", false },
    };

    area.removeFromTop(6);
    const int rowHeight = area.getHeight() / (int)std::size(rows);

    for (const auto& row : rows)
    {
        auto line = area.removeFromTop(rowHeight);

        g.setFont(12.f);
        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.drawText(row.name, line.removeFromLeft(80), juce::Justification::centredLeft);
        g.drawText(row.unit, line.removeFromRight(36), juce::Justification::centredLeft);

        g.setFont(17.f);
        g.setColour(row.warn ? juce::Colours::red : juce::Colours::white);
        g.drawText(row.value, line.withTrimmedRight(6), juce::Justification::centredRight);
    }
}

void LoudnessView::paintHistory(juce::Graphics& g, juce::Rectangle<int> area)
{
    const float reference = getReferenceLufs();
    const float top = reference + rangeAboveTarget, bottom = reference - rangeBelowTarget;

    auto plot = area.withTrimmedLeft(30).toFloat();
    auto yOf = [&](float lufs) { return juce::jmap(juce::jlimit(bottom, top, lufs), bottom, top, plot.getBottom(), plot.getY()); };

    // Draw a grid line every 9 LU
    g.setFont(12.f);
    for (float lufs = bottom; lufs <= top + 0.01f; lufs += 9.f)
    {
        const float y = yOf(lufs);
        g.setColour(juce::Colour(0xff464646));
        g.drawHorizontalLine(juce::roundToInt(y), plot.getX(), plot.getRight());
        g.setColour(juce::Colour(0xff848484));
        g.drawText(juce::String(juce::roundToInt(lufs)), area.getX(), juce::roundToInt(y) - 7, 26, 14, juce::Justification::centredRight);
    }

    // Build the paths of the history, from the oldest point to the newest
    juce::Path shortTermPath, momentaryPath;
    bool shortTermStarted = false, momentaryStarted = false;
    float lastX = plot.getX();

    for (int i = 0; i < historyLength; ++i)
    {
        const auto& point = history[(size_t)((historyIndex + i) % historyLength)];
        const float x = plot.getX() + plot.getWidth() * (float)i / (float)(historyLength - 1);

        if (isLoudness(point.shortTerm))
        {
            if (!shortTermStarted)
                shortTermPath.startNewSubPath(x, plot.getBottom());

            shortTermPath.lineTo(x, yOf(point.shortTerm));
            shortTermStarted = true;
            lastX = x;
        }

        if (isLoudness(point.momentary))
        {
            if (momentaryStarted)
                momentaryPath.lineTo(x, yOf(point.momentary));
            else
                momentaryPath.startNewSubPath(x, yOf(point.momentary));

            momentaryStarted = true;
        }
    }

    {
        juce::Graphics::ScopedSaveState state(g);
        g.reduceClipRegion(plot.toNearestInt());

        if (shortTermStarted)
        {
            // Fill the short-term loudness with a gradient, from HIGHLIGHT_COLOR to BASE_COLOR
            shortTermPath.lineTo(lastX, plot.getBottom());
            shortTermPath.closeSubPath();

            g.setGradientFill(juce::ColourGradient(HIGHLIGHT_COLOR.withAlpha(0.7f), 0.f, plot.getY(),
                BASE_COLOR.withAlpha(0.3f), 0.f, plot.getBottom(), false));
            g.fillPath(shortTermPath);
        }

        g.setColour(juce::Colours::white.withAlpha(0.45f));
        g.strokePath(momentaryPath, juce::PathStrokeType(1.f));

        // The integrated loudness runs across the graph as a line
        if (isLoudness(readings.integrated))
        {
            g.setColour(juce::Colours::white);
            g.drawHorizontalLine(juce::roundToInt(yOf(readings.integrated)), plot.getX(), plot.getRight());
        }

        // The target runs across the graph as a dashed line, with its name at the end
        if (target < 0.f)
        {
            const float dashes[] { 5.f, 4.f };
            const float y = yOf(target);
            g.setColour(juce::Colour(0xff7ddc6f));
            g.drawDashedLine({ plot.getX(), y, plot.getRight(), y }, dashes, 2, 1.2f);

            g.setFont(11.f);
            g.drawText("TARGET", plot.toNearestInt().withY(juce::roundToInt(y) + 2).withHeight(12).reduced(4, 0), juce::Justification::centredRight);
        }
    }

    // Name the lines in their colors
    auto legend = plot.toNearestInt().removeFromTop(16).reduced(6, 0);
    g.setFont(12.f);
    g.setColour(HIGHLIGHT_COLOR);
    g.drawText("SHORT TERM", legend.removeFromLeft(78), juce::Justification::centredLeft);
    g.setColour(juce::Colours::white.withAlpha(0.6f));
    g.drawText("MOMENTARY", legend.removeFromLeft(78), juce::Justification::centredLeft);
    g.setColour(juce::Colours::white);
    g.drawText("INTEGRATED", legend.removeFromLeft(78), juce::Justification::centredLeft);
}
