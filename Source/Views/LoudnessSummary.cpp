#include "LoudnessSummary.h"

//==============================================================================
void LoudnessSummary::update(const LoudnessMeter::Readings& readings, float maxTruePeakDb, float targetLufs, float elapsedSeconds)
{
    secondsSinceReadout += (double)elapsedSeconds;
    if (secondsSinceReadout < Theme::readoutIntervalSeconds)
        return;

    secondsSinceReadout = 0.0;

    const bool hasIntegrated = std::isfinite(readings.integrated);
    const auto newIntegrated = Theme::formatDb(readings.integrated, -200.f);
    const auto newShortTerm = Theme::formatDb(readings.shortTerm, -200.f);
    const auto newRange = std::isfinite(readings.rangeLow) ? juce::String(readings.range, 1) : Theme::formatDb(-300.f);
    const auto newTruePeak = Theme::formatDb(maxTruePeakDb, -150.f);
    const bool newIsOver = maxTruePeakDb > truePeakLimitDb;

    juce::String newDifference;
    if (hasIntegrated && targetLufs < 0.f)
        newDifference = Theme::formatDb(readings.integrated - targetLufs, -200.f) + " LU to target";

    if (newIntegrated != integrated || newDifference != difference || newShortTerm != shortTerm
        || newRange != range || newTruePeak != truePeak || newIsOver != truePeakIsOver)
    {
        integrated = newIntegrated;
        difference = newDifference;
        shortTerm = newShortTerm;
        range = newRange;
        truePeak = newTruePeak;
        truePeakIsOver = newIsOver;
        repaint();
    }
}

void LoudnessSummary::paint(juce::Graphics& g)
{
    g.fillAll(Theme::display);

    auto bounds = getLocalBounds().reduced(14, 8);

    // The integrated loudness is the reading that a delivery is judged by, so it is the largest
    auto top = bounds.removeFromTop(46);
    g.setFont(Theme::labelFont());
    g.setColour(Theme::textDim);
    g.drawText("INTEGRATED", top.removeFromTop(13), juce::Justification::centredLeft);

    auto numberRow = top;
    g.setFont(Theme::font(28.f));
    g.setColour(Theme::accent);
    const int numberWidth = Theme::textWidth(Theme::font(28.f), integrated) + 6;
    g.drawText(integrated, numberRow.removeFromLeft(numberWidth), juce::Justification::centredLeft);

    g.setFont(Theme::labelFont());
    g.setColour(Theme::textDim);
    g.drawText("LUFS", numberRow.removeFromTop(numberRow.getHeight() / 2 + 4), juce::Justification::bottomLeft);
    g.setColour(Theme::held);
    g.drawText(difference, numberRow.expanded(40, 0).withX(numberRow.getX()), juce::Justification::centredLeft);

    // The other readings share a small table
    struct Row { const char* name; const juce::String& value; const char* unit; bool warn; };
    const Row rows[] {
        { "SHORT TERM", shortTerm, "LUFS", false },
        { "RANGE", range, "LU", false },
        { "TRUE PEAK", truePeak, "dBTP", truePeakIsOver },
    };

    bounds.removeFromTop(4);
    const int rowHeight = bounds.getHeight() / (int)std::size(rows);

    for (auto& row : rows)
    {
        auto line = bounds.removeFromTop(rowHeight);

        g.setFont(Theme::labelFont());
        g.setColour(Theme::textDim);
        g.drawText(row.name, line.removeFromLeft(74), juce::Justification::centredLeft);
        g.setColour(Theme::textFaint);
        g.drawText(row.unit, line.removeFromRight(32), juce::Justification::centredLeft);

        g.setFont(Theme::font(14.f));
        g.setColour(row.warn ? Theme::over : Theme::text);
        g.drawText(row.value, line.withTrimmedRight(6), juce::Justification::centredRight);
    }
}

//==============================================================================
void CorrelationBar::resized()
{
    track = getLocalBounds().reduced(14, 0).withTrimmedTop(20).withHeight(8).toFloat();
}

float CorrelationBar::xOf(float correlation) const
{
    return juce::jmap(juce::jlimit(-1.f, 1.f, correlation), -1.f, 1.f, track.getX(), track.getRight());
}

void CorrelationBar::update(float fastCorrelation, float slowCorrelation)
{
    // Only repaint if something has moved by a pixel
    if (juce::roundToInt(xOf(fastCorrelation)) != juce::roundToInt(xOf(fast))
        || juce::roundToInt(xOf(slowCorrelation)) != juce::roundToInt(xOf(slow)))
    {
        fast = fastCorrelation;
        slow = slowCorrelation;
        repaint();
    }
}

void CorrelationBar::paint(juce::Graphics& g)
{
    g.fillAll(Theme::display);

    g.setFont(Theme::labelFont());
    g.setColour(Theme::textDim);
    g.drawText("CORRELATION", getLocalBounds().reduced(14, 0).removeFromTop(18), juce::Justification::centredLeft);

    g.setColour(Theme::track);
    g.fillRect(track);

    // The slow reading as a bar from the center
    const float centre = track.getCentreX();
    const float slowX = xOf(slow);
    g.setColour(slow >= 0.f ? Theme::accent : Theme::over);
    g.fillRect(juce::Rectangle<float>(juce::jmin(centre, slowX), track.getY(), std::abs(slowX - centre), track.getHeight()));

    // The fast reading as a line
    g.setColour(juce::Colours::white);
    g.fillRect(xOf(fast) - 1.f, track.getY() - 2.f, 2.f, track.getHeight() + 4.f);

    g.setColour(Theme::gridStrong);
    g.fillRect(centre - 0.5f, track.getY() - 2.f, 1.f, track.getHeight() + 4.f);

    auto labels = track.toNearestInt().translated(0, 12).withHeight(14);
    g.setColour(Theme::textFaint);
    g.drawText(juce::String(juce::CharPointer_UTF8("\xe2\x88\x92")) + "1", labels, juce::Justification::centredLeft);
    g.drawText("0", labels, juce::Justification::centred);
    g.drawText("+1", labels, juce::Justification::centredRight);
}
