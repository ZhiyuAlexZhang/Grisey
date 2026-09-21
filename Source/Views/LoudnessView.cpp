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
    paintHeader(g, area.removeFromTop(18));
    area.removeFromTop(8);
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

void LoudnessView::paintHeader(juce::Graphics& g, juce::Rectangle<int> area)
{
    // The header ends where the plot does, short of the labels of the scale
    area.removeFromRight(scaleWidth);

    const auto labelFont = Theme::labelFont();
    const auto valueFont = Theme::font(14.f);

    // The readings that only this view has, from the right. The integrated and the short-term loudness,
    // the range and the true peak are in the side column, whichever view is showing, so they are not
    // repeated here.
    const bool hasIntegrated = isLoudness(shown.readings.integrated);
    const bool hasShortTerm = isLoudness(shown.readings.shortTerm);
    const bool hasTruePeak = shown.maxTruePeak > -150.f;

    struct Readout
    {
        const char* name;
        juce::String value;
        const char* unit;
    };

    const Readout readouts[] {
        { "PSR", format(shown.recentTruePeak - shown.readings.shortTerm, shown.recentTruePeak > -150.f && hasShortTerm), "LU" },
        { "PLR", format(shown.maxTruePeak - shown.readings.integrated, hasTruePeak && hasIntegrated), "LU" },
        { "MOMENTARY", format(shown.readings.momentary, isLoudness(shown.readings.momentary)), "LUFS" },
    };

    for (const auto& readout : readouts)
    {
        g.setFont(labelFont);
        g.setColour(Theme::textFaint);
        g.drawText(readout.unit, area.removeFromRight(Theme::textWidth(labelFont, readout.unit) + 2), juce::Justification::centredRight);

        g.setFont(valueFont);
        g.setColour(Theme::text);
        g.drawText(readout.value, area.removeFromRight(44).withTrimmedRight(5), juce::Justification::centredRight);

        g.setFont(labelFont);
        g.setColour(Theme::textDim);
        g.drawText(readout.name, area.removeFromRight(Theme::textWidth(labelFont, readout.name) + 2), juce::Justification::centredRight);

        area.removeFromRight(16);
    }

    // Name the lines of the graph in their colors, in what is left
    struct Line
    {
        const char* name;
        juce::Colour colour;
    };

    const Line lines[] {
        { "SHORT TERM", Theme::accent },
        { "MOMENTARY", Theme::second },
        { "INTEGRATED", juce::Colours::white },
    };

    g.setFont(labelFont);
    for (const auto& line : lines)
    {
        const int width = Theme::textWidth(labelFont, line.name) + 16;
        if (width > area.getWidth())
            break;

        g.setColour(line.colour);
        g.drawText(line.name, area.removeFromLeft(width), juce::Justification::centredLeft);
    }
}

void LoudnessView::paintHistory(juce::Graphics& g, juce::Rectangle<int> area)
{
    const float top = maxLufs, bottom = minLufs;

    auto plot = area.withTrimmedRight(scaleWidth).withTrimmedBottom(18).toFloat();
    auto yOf = [&](float lufs) { return juce::jmap(juce::jlimit(bottom, top, lufs), bottom, top, plot.getBottom(), plot.getY()); };

    // Draw a grid line every 6 LU, and label the levels down the right
    g.setFont(Theme::font(10.5f));
    for (float lufs = bottom; lufs <= top + 0.01f; lufs += gridStepLu)
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
}
