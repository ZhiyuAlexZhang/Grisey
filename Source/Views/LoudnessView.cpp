#include "LoudnessView.h"

namespace
{
    bool isLoudness(float value)
    {
        return std::isfinite(value);
    }

    // A reading with one decimal place, or a dash when there is none
    juce::String format(float value, bool valid)
    {
        return valid ? Theme::formatDb(value, -1000.f).replace("+", "") : Theme::formatDb(-2000.f, -1000.f);
    }
}

//==============================================================================
LoudnessView::LoudnessView()
{
    setOpaque(true);
    recentTruePeaks.fill(-200.f);
}

void LoudnessView::paint(juce::Graphics& g)
{
    Theme::fillDisplay(g, getLocalBounds());

    auto area = getLocalBounds().reduced(18, 14);
    paintReadouts(g, area.removeFromLeft(210));
    area.removeFromLeft(18);
    paintHistory(g, area);
}

void LoudnessView::update(const LoudnessMeter::Readings& newReadings, float truePeakDb, float maxTruePeakDb,
                          float targetLufs, int numNewSlots, float elapsedSeconds, bool readoutDue)
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
        recentTruePeaks[recentTruePeakIndex] = -200.f;
    }

    recentTruePeaks[recentTruePeakIndex] = juce::jmax(recentTruePeaks[recentTruePeakIndex], truePeakDb);
    recentTruePeak = *std::max_element(recentTruePeaks.begin(), recentTruePeaks.end());

    for (int slot = 0; slot < numNewSlots; ++slot)
        history.push({ readings.shortTerm, readings.momentary });

    // The view is drawn again at every readout, which is as often as its numbers change
    if (readoutDue)
    {
        shown = { readings, maxTruePeak, recentTruePeak };
        repaint();
    }
}

void LoudnessView::clearHistory()
{
    history.clear();
    repaint();
}

void LoudnessView::setSpan(float seconds)
{
    if (!juce::exactlyEqual(seconds, spanSeconds))
    {
        spanSeconds = seconds;
        repaint();
    }
}

float LoudnessView::getReferenceLufs() const
{
    // Without a target the scale sits where it would for EBU R 128
    return target < 0.f ? target : -23.f;
}

void LoudnessView::paintReadouts(juce::Graphics& g, juce::Rectangle<int> area)
{
    const bool hasIntegrated = isLoudness(shown.readings.integrated);
    const bool hasShortTerm = isLoudness(shown.readings.shortTerm);
    const bool hasTruePeak = shown.maxTruePeak > -150.f;

    // The integrated loudness is the reading that a delivery is judged by, so it is the largest
    auto top = area.removeFromTop(84);
    g.setFont(Theme::labelFont());
    g.setColour(Theme::textDim);
    g.drawText("INTEGRATED", top.removeFromTop(14), juce::Justification::centredLeft);

    g.setFont(Theme::font(48.f));
    g.setColour(Theme::accent);
    g.drawText(format(shown.readings.integrated, hasIntegrated), top.removeFromTop(50), juce::Justification::centredLeft);

    g.setFont(Theme::labelFont());
    g.setColour(Theme::textDim);
    auto unitRow = top;
    g.drawText("LUFS", unitRow.removeFromLeft(36), juce::Justification::centredLeft);

    if (hasIntegrated && target < 0.f)
    {
        g.setFont(Theme::font(11.f));
        g.setColour(Theme::text);
        g.drawText(Theme::formatDb(shown.readings.integrated - target, -1000.f) + " LU to target", unitRow, juce::Justification::centredLeft);
    }

    // The other readings share a table
    struct Row
    {
        const char* name;
        juce::String value;
        const char* unit;
        bool warn;
    };

    const Row rows[] {
        { "SHORT TERM", format(shown.readings.shortTerm, hasShortTerm), "LUFS", false },
        { "MOMENTARY", format(shown.readings.momentary, isLoudness(shown.readings.momentary)), "LUFS", false },
        { "RANGE", format(shown.readings.range, isLoudness(shown.readings.rangeLow)), "LU", false },
        // A true peak keeps its sign, because whether it is over or under full scale is the point of it
        { "TRUE PEAK", Theme::formatDb(hasTruePeak ? shown.maxTruePeak : -2000.f, -1000.f), "dBTP", hasTruePeak && shown.maxTruePeak > truePeakLimitDb },
        { "PLR", format(shown.maxTruePeak - shown.readings.integrated, hasTruePeak && hasIntegrated), "LU", false },
        { "PSR", format(shown.recentTruePeak - shown.readings.shortTerm, shown.recentTruePeak > -150.f && hasShortTerm), "LU", false },
    };

    area.removeFromTop(10);
    const int rowHeight = juce::jmin(34, area.getHeight() / (int)std::size(rows));

    for (const auto& row : rows)
    {
        auto line = area.removeFromTop(rowHeight);

        g.setColour(Theme::grid);
        g.fillRect(line.removeFromTop(1));

        g.setFont(Theme::labelFont());
        g.setColour(Theme::textDim);
        g.drawText(row.name, line.removeFromLeft(90), juce::Justification::centredLeft);
        g.setColour(Theme::textFaint);
        g.drawText(row.unit, line.removeFromRight(36), juce::Justification::centredLeft);

        g.setFont(Theme::font(18.f));
        g.setColour(row.warn ? Theme::over : Theme::text);
        g.drawText(row.value, line.withTrimmedRight(8), juce::Justification::centredRight);
    }
}

void LoudnessView::paintHistory(juce::Graphics& g, juce::Rectangle<int> area)
{
    const float reference = getReferenceLufs();
    const float top = reference + rangeAboveTarget, bottom = reference - rangeBelowTarget;

    auto plot = area.withTrimmedRight(34).withTrimmedBottom(18).toFloat();
    auto yOf = [&](float lufs) { return juce::jmap(juce::jlimit(bottom, top, lufs), bottom, top, plot.getBottom(), plot.getY()); };

    // Draw a grid line every 9 LU, and label the levels down the right
    g.setFont(Theme::font(10.5f));
    for (float lufs = bottom; lufs <= top + 0.01f; lufs += 9.f)
    {
        const float y = yOf(lufs);
        g.setColour(Theme::grid);
        g.fillRect(plot.getX(), y, plot.getWidth(), 1.f);
        g.setColour(Theme::accent.withAlpha(0.55f));
        g.drawText(juce::String(juce::roundToInt(lufs)), juce::Rectangle<float>(plot.getRight() + 4.f, y - 7.f, 30.f, 14.f), juce::Justification::centredLeft);
    }

    Timeline::drawTimeAxis(g, plot.toNearestInt(), spanSeconds);

    // Build the paths of the history, from the oldest slot of the span to the newest. The loudness
    // changes ten times a second, so every third slot is enough to draw it.
    juce::Path shortTermPath, momentaryPath;
    bool shortTermStarted = false, momentaryStarted = false;
    float lastX = plot.getX();

    const int numSlots = Timeline::slotsIn(spanSeconds);
    const int step = 3;

    for (int age = numSlots - 1; age >= 0; age -= (age > step ? step : 1))
    {
        const auto* point = history.fromNewest(age);
        if (point == nullptr)
            continue;

        const float x = plot.getRight() - plot.getWidth() * (float)age / (float)numSlots;

        if (isLoudness(point->shortTerm))
        {
            if (!shortTermStarted)
                shortTermPath.startNewSubPath(x, plot.getBottom());

            shortTermPath.lineTo(x, yOf(point->shortTerm));
            shortTermStarted = true;
            lastX = x;
        }

        if (isLoudness(point->momentary))
        {
            if (momentaryStarted)
                momentaryPath.lineTo(x, yOf(point->momentary));
            else
                momentaryPath.startNewSubPath(x, yOf(point->momentary));

            momentaryStarted = true;
        }
    }

    {
        juce::Graphics::ScopedSaveState state(g);
        g.reduceClipRegion(plot.toNearestInt());

        if (shortTermStarted)
        {
            // A flat, translucent fill beneath a solid line, as in the spectrum
            auto outline = shortTermPath;
            shortTermPath.lineTo(lastX, plot.getBottom());
            shortTermPath.closeSubPath();

            g.setColour(Theme::accent.withAlpha(Theme::accentFillAlpha));
            g.fillPath(shortTermPath);

            // The outline starts at the bottom of the plot, so its first segment is left out
            juce::Path line;
            juce::Path::Iterator segments(outline);
            bool started = false;
            while (segments.next())
            {
                if (segments.elementType == juce::Path::Iterator::lineTo)
                {
                    if (started) line.lineTo(segments.x1, segments.y1);
                    else         line.startNewSubPath(segments.x1, segments.y1);
                    started = true;
                }
            }

            g.setColour(Theme::accent);
            g.strokePath(line, juce::PathStrokeType(Theme::curveThickness, juce::PathStrokeType::curved));
        }

        g.setColour(Theme::second);
        g.strokePath(momentaryPath, juce::PathStrokeType(1.2f));

        // The integrated loudness runs across the graph as a line
        if (isLoudness(readings.integrated))
        {
            g.setColour(juce::Colours::white);
            g.fillRect(plot.getX(), yOf(readings.integrated), plot.getWidth(), 1.f);
        }

        // The target runs across the graph as a dashed line, with its name at the end
        if (target < 0.f)
        {
            const float dashes[] { 5.f, 4.f };
            const float y = yOf(target);
            g.setColour(Theme::target);
            g.drawDashedLine({ plot.getX(), y, plot.getRight(), y }, dashes, 2, 1.2f);

            g.setFont(Theme::labelFont());
            g.drawText("TARGET", plot.toNearestInt().withY(juce::roundToInt(y) + 2).withHeight(12).reduced(6, 0), juce::Justification::centredRight);
        }
    }

    // Name the lines in their colors
    auto legend = plot.toNearestInt().withTrimmedLeft(10).withTrimmedTop(6).removeFromTop(14);
    g.setFont(Theme::labelFont());
    g.setColour(Theme::accent);
    g.drawText("SHORT TERM", legend.removeFromLeft(84), juce::Justification::centredLeft);
    g.setColour(Theme::second);
    g.drawText("MOMENTARY", legend.removeFromLeft(84), juce::Justification::centredLeft);
    g.setColour(juce::Colours::white);
    g.drawText("INTEGRATED", legend.removeFromLeft(84), juce::Justification::centredLeft);
}
