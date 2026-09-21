#include "SpectrumView.h"

//==============================================================================
SpectrumView::SpectrumView(SpectrumSource& spectrumSource) : source(spectrumSource)
{
    setOpaque(true);
    display.minFrequency = minFrequency;
    display.maxFrequency = maxFrequency;
}

float SpectrumView::xOf(double frequency) const
{
    const double proportion = std::log(frequency / minFrequency) / std::log(maxFrequency / minFrequency);
    return (float)plot.getX() + (float)proportion * (float)plot.getWidth();
}

double SpectrumView::frequencyAt(float x) const
{
    const double proportion = (double)(x - (float)plot.getX()) / (double)juce::jmax(1, plot.getWidth());
    return minFrequency * std::pow(maxFrequency / minFrequency, juce::jlimit(0.0, 1.0, proportion));
}

float SpectrumView::yOf(float decibels) const
{
    return juce::jmap(decibels, minDecibels, maxDecibels, (float)plot.getBottom(), (float)plot.getY());
}

void SpectrumView::resized()
{
    // The levels are labelled down the right, and the frequencies along the bottom
    plot = getLocalBounds().withTrimmedRight(34).withTrimmedBottom(20).withTrimmedTop(8).withTrimmedLeft(8);
    grid.invalidate();
}

void SpectrumView::paintGrid(juce::Graphics& g)
{
    Theme::fillDisplay(g, getLocalBounds());
    g.setFont(Theme::font(10.5f));

    // A line for every first digit of the frequency, stronger at the decades
    for (double decade = 10.0; decade < maxFrequency; decade *= 10.0)
    {
        for (int digit = 1; digit <= 9; ++digit)
        {
            const double frequency = decade * digit;
            if (frequency < minFrequency || frequency > maxFrequency)
                continue;

            g.setColour(digit == 1 ? Theme::gridStrong : Theme::grid);
            g.fillRect(xOf(frequency), (float)plot.getY(), 1.f, (float)plot.getHeight());

            if (digit == 1 || digit == 2 || digit == 5)
            {
                const auto text = frequency < 1000.0 ? juce::String((int)frequency) : juce::String((int)(frequency / 1000.0)) + "k";
                g.setColour(digit == 1 ? Theme::textDim : Theme::textFaint);
                const float centreX = juce::jlimit((float)plot.getX() + 10.f, (float)plot.getRight() - 10.f, xOf(frequency));
                g.drawText(text, juce::Rectangle<float>(40.f, 14.f).withCentre({ centreX, (float)plot.getBottom() + 10.f }), juce::Justification::centred);
            }
        }
    }

    // A line for every 12 dB, stronger at 0
    for (int decibels = 0; decibels > (int)minDecibels; decibels -= 12)
    {
        const float y = yOf((float)decibels);
        g.setColour(decibels == 0 ? Theme::gridStrong : Theme::grid);
        g.fillRect((float)plot.getX(), y, (float)plot.getWidth(), 1.f);

        // The scale of levels is in the color of the curve that it measures
        g.setColour(Theme::accent.withAlpha(decibels == 0 ? 0.9f : 0.55f));
        g.drawText(juce::String(decibels), juce::Rectangle<float>((float)plot.getRight() + 4.f, y - 7.f, 28.f, 14.f), juce::Justification::centredLeft);
    }
}

void SpectrumView::paint(juce::Graphics& g)
{
    grid.draw(g, getLocalBounds(), true, [this](juce::Graphics& layer) { paintGrid(layer); });

    if (shown[0].empty())
        return;

    {
        juce::Graphics::ScopedSaveState state(g);
        g.reduceClipRegion(plot);

        // The second curve (right or side) is drawn first, so that the first lies on top of it
        const std::array<juce::Colour, 2> colours { Theme::accent, Theme::second };
        for (int index = 1; index >= 0; --index)
        {
            const auto i = (size_t)index;

            // Where the curve was a moment ago, fainter the longer ago it was
            for (int age = numTrails; age >= 1; --age)
            {
                const auto& trail = trails[(size_t)((nextTrail - age + 2 * numTrails) % numTrails)][i];
                if (trail.size() == shown[i].size())
                {
                    g.setColour(colours[i].withAlpha(0.42f * (float)(numTrails + 1 - age) / (float)(numTrails + 1)));
                    g.strokePath(makePath(trail, false, trailPointStep), juce::PathStrokeType(1.f));
                }
            }

            // A glow beneath the line, which fades out towards the bottom. The second curve usually
            // lies over the first, as left and right do, so its glow is fainter, or the two would
            // mix into a muddy color.
            g.setGradientFill(juce::ColourGradient(colours[i].withAlpha(index == 0 ? 0.34f : 0.12f), 0.f, (float)plot.getY(),
                                                   colours[i].withAlpha(0.f), 0.f, (float)plot.getBottom(), false));
            g.fillPath(makePath(shown[i], true));

            g.setColour(colours[i]);
            g.strokePath(makePath(shown[i], false), juce::PathStrokeType(1.5f, juce::PathStrokeType::curved));

            // The peak hold is a thin, paler line of the same color
            if (settings.peakHold && peakHolds[i].size() == shown[i].size())
            {
                g.setColour(colours[i].brighter(0.6f).withAlpha(0.75f));
                g.strokePath(makePath(peakHolds[i], false), juce::PathStrokeType(1.f));
            }
        }

        // The correlation strip runs along the bottom of the plot
        if (correlationStrip.isValid())
        {
            g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
            g.drawImage(correlationStrip, plot.toFloat().removeFromBottom(4.f), juce::RectanglePlacement::stretchToFit);
        }
    }

    // Name the curves in their colors
    auto legend = plot.withTrimmedLeft(10).withTrimmedTop(6).removeFromTop(14);
    g.setFont(Theme::labelFont());
    g.setColour(Theme::accent);
    g.drawText(settings.midSide ? "MID" : "LEFT", legend.removeFromLeft(settings.midSide ? 32 : 36), juce::Justification::centredLeft);
    g.setColour(Theme::second);
    g.drawText(settings.midSide ? "SIDE" : "RIGHT", legend.removeFromLeft(44), juce::Justification::centredLeft);

    if (hover.has_value())
        paintReadout(g);
}

void SpectrumView::paintReadout(juce::Graphics& g)
{
    const auto position = *hover;
    const double frequency = frequencyAt((float)position.x);

    g.setColour(Theme::text.withAlpha(0.25f));
    g.fillRect((float)position.x, (float)plot.getY(), 1.f, (float)plot.getHeight());

    // The level of the first curve at this frequency
    const auto index = (size_t)juce::jlimit(0, (int)shown[0].size() - 1,
        juce::roundToInt((float)(position.x - plot.getX()) / (float)juce::jmax(1, plot.getWidth()) * (float)(shown[0].size() - 1)));
    const float level = shown[0][index];

    // A point on the curve, bright with a dark outline
    const auto point = juce::Rectangle<float>(10.f, 10.f).withCentre({ (float)position.x, yOf(juce::jmax(level, minDecibels)) });
    g.setColour(Theme::accent);
    g.fillEllipse(point);
    g.setColour(juce::Colour(0xff05020a).withAlpha(0.45f));
    g.drawEllipse(point, 1.f);

    const auto text = Theme::formatFrequency(frequency) + "    " + Theme::formatNote(frequency) + "    " + Theme::formatDb(level, minDecibels) + " dB";
    const auto font = Theme::controlFont();
    auto box = juce::Rectangle<int>(Theme::textWidth(font, text) + 20, 24).withPosition(position.x + 12, plot.getY() + 26);

    // Keep the readout within the plot
    if (box.getRight() > plot.getRight())
        box.setX(position.x - 12 - box.getWidth());

    Theme::drawPanel(g, box.toFloat());
    g.setFont(font);
    g.setColour(Theme::text);
    g.drawText(text, box, juce::Justification::centred);
}

void SpectrumView::update(bool hasNewSpectra, const Settings& newSettings, float elapsedSeconds, bool held)
{
    // One point per pixel is as fine as the screen can show
    const int numPoints = juce::jmax(2, plot.getWidth());
    const bool settingsChanged = numPoints != display.numPoints || !(newSettings == settings);

    // Whatever is no longer comparable starts afresh
    if (settingsChanged)
    {
        for (auto& hold : peakHolds)
            hold.clear();

        for (auto& trail : trails)
            for (auto& curve : trail)
                curve.clear();
    }

    settings = newSettings;
    display.numPoints = numPoints;
    display.tiltDbPerOctave = settings.tiltDbPerOctave;
    display.smoothingOctaves = settings.smoothingOctaves;

    if ((hasNewSpectra && !held) || settingsChanged)
    {
        auto& engine = source.getEngine();
        const auto sampleRate = source.getSampleRate();

        engine.render(settings.midSide ? SpectrumEngine::Curve::mid : SpectrumEngine::Curve::left, display, sampleRate, curves[0]);
        engine.render(settings.midSide ? SpectrumEngine::Curve::side : SpectrumEngine::Curve::right, display, sampleRate, curves[1]);

        if (settings.peakHold)
        {
            for (size_t i = 0; i < curves.size(); ++i)
            {
                if (peakHolds[i].size() != curves[i].size())
                    peakHolds[i] = curves[i];

                for (size_t point = 0; point < curves[i].size(); ++point)
                    peakHolds[i][point] = juce::jmax(peakHolds[i][point], curves[i][point]);
            }
        }

        // The correlation strip has one pixel per display point: blue where the channels are in phase,
        // red where they are out of phase, and clear where they are unrelated or silent
        engine.renderCorrelation(display, sampleRate, correlationBandOctaves, correlationQuietDb, correlation);

        if (correlationStrip.getWidth() != numPoints)
            correlationStrip = juce::Image(juce::Image::ARGB, numPoints, 1, true);

        juce::Image::BitmapData pixels(correlationStrip, juce::Image::BitmapData::writeOnly);
        for (int point = 0; point < numPoints; ++point)
        {
            const float value = correlation[(size_t)point];
            const auto colour = std::isnan(value) ? juce::Colours::transparentBlack
                : (value >= 0.f ? Theme::second : Theme::over).withAlpha(std::abs(value));
            pixels.setPixelColour(point, 0, colour);
        }
    }

    if (held || curves[0].empty())
    {
        if (settingsChanged)
            repaint();

        return;
    }

    // Move what is drawn towards the latest spectrum: quickly up, and slowly down
    const float attack = 1.f - std::exp(-elapsedSeconds / attackSeconds);
    const float release = 1.f - std::exp(-elapsedSeconds / releaseSeconds);
    float furthestMoved = 0.f;

    for (size_t i = 0; i < curves.size(); ++i)
    {
        // A curve that is new, or of a new length, starts where it is heading
        if (shown[i].size() != curves[i].size())
        {
            shown[i] = curves[i];
            furthestMoved = 100.f;
            continue;
        }

        for (size_t point = 0; point < curves[i].size(); ++point)
        {
            // Below the bottom of the plot there is nothing to see, so nothing to wait for
            const float target = juce::jmax(curves[i][point], minDecibels - 6.f);
            const float from = juce::jmax(shown[i][point], minDecibels - 6.f);
            const float to = from + (target - from) * (target > from ? attack : release);

            furthestMoved = juce::jmax(furthestMoved, std::abs(to - from));
            shown[i][point] = to;
        }
    }

    // Keep a copy of where the curves are, every so often, for the trails
    secondsSinceTrail += elapsedSeconds;
    if (secondsSinceTrail >= trailIntervalSeconds)
    {
        secondsSinceTrail = 0.f;
        trails[(size_t)nextTrail] = shown;
        nextTrail = (nextTrail + 1) % numTrails;
    }

    // A twentieth of a decibel is less than a pixel
    if (furthestMoved > 0.05f || settingsChanged)
        repaint();
}

juce::Path SpectrumView::makePath(const std::vector<float>& decibels, bool closed, int pointStep) const
{
    juce::Path path;
    if (decibels.size() < 2)
        return path;

    path.preallocateSpace(3 * (int)decibels.size() + 12);

    const float xStep = (float)plot.getWidth() / (float)(decibels.size() - 1);
    const auto step = (size_t)juce::jmax(1, pointStep);
    for (size_t point = 0; point < decibels.size(); point += step)
    {
        // A point that stands for several takes the highest of them, so that a narrow peak is kept
        float highest = decibels[point];
        for (size_t other = point + 1; other < juce::jmin(point + step, decibels.size()); ++other)
            highest = juce::jmax(highest, decibels[other]);

        // A little below the bottom, so that the stroke of a silent curve is out of sight
        const float y = yOf(juce::jmax(highest, minDecibels - 3.f));
        const float x = (float)plot.getX() + xStep * (float)point;

        if (point == 0)
            path.startNewSubPath(x, y);
        else
            path.lineTo(x, y);
    }

    if (closed)
    {
        path.lineTo((float)plot.getRight(), (float)plot.getBottom() + 4.f);
        path.lineTo((float)plot.getX(), (float)plot.getBottom() + 4.f);
        path.closeSubPath();
    }

    return path;
}

void SpectrumView::mouseMove(const juce::MouseEvent& e)
{
    const auto position = e.getPosition();
    const std::optional<juce::Point<int>> newHover = plot.contains(position) ? std::optional(position) : std::nullopt;

    if (newHover != hover)
    {
        hover = newHover;
        repaint();
    }
}

void SpectrumView::mouseExit(const juce::MouseEvent&)
{
    if (hover.has_value())
    {
        hover.reset();
        repaint();
    }
}

void SpectrumView::mouseDown(const juce::MouseEvent&)
{
    resetPeakHold();
}

void SpectrumView::resetPeakHold()
{
    for (auto& hold : peakHolds)
        hold.clear();
}
