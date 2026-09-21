#include "GoniometerView.h"

//==============================================================================
GoniometerView::GoniometerView(juce::AudioProcessorValueTreeState& apvts, const juce::String& scaleParameterID) :
    scaleKnob(apvts, scaleParameterID, "Scale", "%")
{
    setOpaque(true);
    samples.setSize(2, maxSamplesPerUpdate, false, true, true);
    addAndMakeVisible(scaleKnob);

    for (int i = 0; i < toneMapSize; ++i)
    {
        const float light = toneMapMaxIntensity * (float)i / (float)(toneMapSize - 1);
        const float brightness = 1.f - std::exp(-0.9f * light);
        const float whiteness = juce::jmax(0.f, brightness - 0.6f) * 1.5f;
        const auto colour = Theme::accent.interpolatedWith(juce::Colours::white, whiteness);

        // The pixels of an ARGB image hold premultiplied colors
        toneMap[(size_t)i].setARGB((juce::uint8)(brightness * 255.f), colour.getRed(), colour.getGreen(), colour.getBlue());
        toneMap[(size_t)i].premultiply();
    }
}

void GoniometerView::resized()
{
    // The plot is the largest square that leaves room for the labels around it
    const int side = juce::jmax(0, juce::jmin(getWidth(), getHeight()) - 56);
    plot = juce::Rectangle<int>(side, side).withCentre(getLocalBounds().getCentre());

    gridSize = juce::jmin(maxGridSize, side * cellsPerPixel);

    const float cellsPerPoint = side > 0 ? (float)gridSize / (float)side : (float)cellsPerPixel;
    lightScale = juce::square(cellsPerPoint / (float)cellsPerPixel);
    intensity.assign((size_t)(gridSize * gridSize), 0.f);
    image = gridSize > 0 ? juce::Image(juce::Image::ARGB, gridSize, gridSize, true) : juce::Image();
    isLit = false;
    hasLastPoint = false;

    scaleKnob.setBounds(getLocalBounds().removeFromBottom(128).removeFromLeft(108).reduced(4, 4));
    background.invalidate();
}

void GoniometerView::paintBackground(juce::Graphics& g)
{
    Theme::fillDisplay(g, getLocalBounds());

    const auto centre = plot.getCentre().toFloat();
    const float radius = 0.5f * (float)plot.getWidth();

    // Rings at the edge and at half of it, which is 6 dB down
    g.setColour(Theme::gridStrong);
    g.drawEllipse(juce::Rectangle<float>(2.f * radius, 2.f * radius).withCentre(centre), 1.f);
    g.setColour(Theme::grid);
    g.drawEllipse(juce::Rectangle<float>(radius, radius).withCentre(centre), 1.f);

    // The axes and their labels: side runs left and right, mid runs up,
    // and the channels lie on the diagonals in between
    const std::array<const char*, 5> labels { "S", "L", "M", "R", "S" };
    g.setFont(Theme::labelFont());

    for (int i = 0; i < 8; ++i)
    {
        // The polar plot only uses the upper half
        if (mode == polar && i > 4)
            continue;

        // Starting on the left and turning clockwise
        const float angle = (float)i * juce::MathConstants<float>::pi / 4.f - juce::MathConstants<float>::halfPi;
        g.setColour(i % 2 == 0 ? Theme::gridStrong : Theme::grid);
        g.drawLine(juce::Line<float>(centre, centre.getPointOnCircumference(radius, angle)), 1.f);

        if (i < (int)labels.size())
        {
            g.setColour(i == 1 || i == 3 ? Theme::text : Theme::textDim);
            g.drawText(labels[(size_t)i], juce::Rectangle<float>(24.f, 16.f).withCentre(centre.getPointOnCircumference(radius + 14.f, angle)), juce::Justification::centred);
        }
    }
}

void GoniometerView::paint(juce::Graphics& g)
{
    background.draw(g, getLocalBounds(), true, [this](juce::Graphics& layer) { paintBackground(layer); });

    if (image.isValid() && isLit)
    {
        // Keep the trace within the circle
        juce::Path circle;
        circle.addEllipse(plot.toFloat());

        juce::Graphics::ScopedSaveState state(g);
        g.reduceClipRegion(circle);
        g.drawImage(image, plot.toFloat(), juce::RectanglePlacement::stretchToFit);
    }
}

void GoniometerView::update(const SampleRingBuffer& ringBuffer, float elapsedSeconds, Mode newMode, float persistenceSeconds, float newScale)
{
    scaleKnob.setEmphasized(isMouseOver(true));
    scale = newScale;

    if (gridSize == 0)
        return;

    // A change of mode starts a new trace, on a new background
    if (newMode != mode)
    {
        mode = newMode;
        std::fill(intensity.begin(), intensity.end(), 0.f);
        hasLastPoint = false;
        isLit = false;
        background.invalidate();
        repaint();
    }

    // Plot the samples that have arrived since the last update
    const auto totalWritten = ringBuffer.getTotalWritten();
    const auto numNew = totalWritten >= lastTotalWritten ? totalWritten - lastTotalWritten : totalWritten;
    const int numSamples = (int)juce::jmin<juce::uint64>(numNew, (juce::uint64)maxSamplesPerUpdate);

    if (!isLit && numSamples == 0)
        return;

    // Let the light that is already there fade, and find out whether any is left
    const float keep = persistenceSeconds > 0.f ? std::exp(-elapsedSeconds / persistenceSeconds) : 0.f;
    float brightest = 0.f;
    for (auto& value : intensity)
    {
        value *= keep;
        brightest = juce::jmax(brightest, value);
    }

    // Light too faint to show in an 8-bit image counts as none
    const bool wasLit = isLit;
    isLit = brightest > faintestLight;
    if (!isLit)
        std::fill(intensity.begin(), intensity.end(), 0.f);

    // The trace only carries on from the last update if no samples were left out in between
    if (numNew > (juce::uint64)maxSamplesPerUpdate)
        hasLastPoint = false;

    if (numSamples > 0 && ringBuffer.readLatest(samples.getWritePointer(0), samples.getWritePointer(1), numSamples))
    {
        lastTotalWritten = totalWritten;

        // A full-scale mono signal, whose mid is 2, reaches the edge of the circle at a scale of 1
        const float half = 0.5f * (float)gridSize;
        const float gain = scale * half * 0.5f;
        const auto* left = samples.getReadPointer(0);
        const auto* right = samples.getReadPointer(1);

        for (int i = 0; i < numSamples; ++i)
        {
            float side = (left[i] - right[i]) * gain;
            float mid = (left[i] + right[i]) * gain;

            if (!std::isfinite(side) || !std::isfinite(mid))
            {
                hasLastPoint = false;
                continue;
            }

            // Silence draws nothing, rather than a dot in the middle
            if (juce::exactlyEqual(side, 0.f) && juce::exactlyEqual(mid, 0.f))
            {
                hasLastPoint = false;
                continue;
            }

            // The polar plot folds the lower half onto the upper half, through the center
            if (mode == polar && mid < 0.f)
            {
                side = -side;
                mid = -mid;
            }

            // The left channel leans to the left, and mid points up
            const juce::Point<float> point { half - side, half - mid };

            // The fold breaks the trace up, so the polar plot is made of dots rather than lines
            if (mode == polar || !hasLastPoint)
                addLight(juce::roundToInt(point.x), juce::roundToInt(point.y), dotLight * lightScale);
            else
                addLine(lastPoint, point);

            lastPoint = point;
            hasLastPoint = true;
            isLit = true;
        }
    }

    if (isLit)
        renderImage();

    if (isLit || wasLit)
        repaint(plot);
}

void GoniometerView::addLine(juce::Point<float> from, juce::Point<float> to)
{
    // The light of one sample is spread along the path of the beam, so a fast beam leaves a dim trace
    const float length = juce::jmax(std::abs(to.x - from.x), std::abs(to.y - from.y));
    const int steps = juce::jlimit(1, 4 * gridSize, (int)std::ceil(length));
    const float amount = lineLight * lightScale / (float)steps;

    for (int step = 1; step <= steps; ++step)
    {
        const float proportion = (float)step / (float)steps;
        addLight(juce::roundToInt(from.x + (to.x - from.x) * proportion),
                 juce::roundToInt(from.y + (to.y - from.y) * proportion), amount);
    }
}

void GoniometerView::addLight(int x, int y, float amount)
{
    if (x >= 0 && y >= 0 && x < gridSize && y < gridSize)
        intensity[(size_t)(y * gridSize + x)] += amount;
}

void GoniometerView::renderImage()
{
    juce::Image::BitmapData pixels(image, juce::Image::BitmapData::writeOnly);
    const float toIndex = (float)(toneMapSize - 1) / toneMapMaxIntensity;

    for (int y = 0; y < gridSize; ++y)
    {
        const float* row = intensity.data() + (size_t)(y * gridSize);
        auto* line = pixels.getLinePointer(y);

        for (int x = 0; x < gridSize; ++x)
        {
            const int index = juce::jmin(toneMapSize - 1, (int)(row[x] * toIndex));
            *((juce::PixelARGB*)(line + x * pixels.pixelStride)) = toneMap[(size_t)index];
        }
    }
}
