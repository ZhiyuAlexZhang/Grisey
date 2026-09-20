#include "Goniometer.h"

//==============================================================================
// Implementation for the Goniometer class
Goniometer::Goniometer()
{
    // Initialize the internal buffer and clear it
    internalBuffer.setSize(2, maxSamplesPerUpdate, false, true, true);
    internalBuffer.clear();
    // Initialize the scaling factor
    scale = 1;
}

void Goniometer::paint(juce::Graphics& g)
{
    // Draw the background of the goniometer
    drawBackground(g);

    if (image.isValid())
    {
        // Keep the trace within the circle
        juce::Path circle;
        circle.addEllipse((float)center.getX() - getRadius(), (float)center.getY() - getRadius(), 2.f * getRadius(), 2.f * getRadius());

        juce::Graphics::ScopedSaveState state(g);
        g.reduceClipRegion(circle);
        g.drawImage(image, circle.getBounds(), juce::RectanglePlacement::stretchToFit);
    }
}

void Goniometer::resized()
{
    // Update the center point of the component based on its new dimensions
    center = juce::Point<int>(getWidth() / 2, getHeight() / 2);
    // Update the width and height variables used for drawing the background
    w = getWidth() - 40;
    h = getHeight() - 40;

    // The grid covers the square around the circle
    gridSize = juce::jmax(0, w * cellsPerPixel);
    intensity.assign((size_t)(gridSize * gridSize), 0.f);
    image = gridSize > 0 ? juce::Image(juce::Image::ARGB, gridSize, gridSize, true) : juce::Image();
    hasLastPoint = false;
}

void Goniometer::update(const SampleRingBuffer& ringBuffer, float elapsedSeconds, Mode newMode, float persistenceSeconds)
{
    if (gridSize == 0)
        return;

    // A change of mode starts a new trace
    if (newMode != mode)
    {
        mode = newMode;
        std::fill(intensity.begin(), intensity.end(), 0.f);
        hasLastPoint = false;
        repaint();
    }

    // Let the light that is already there fade
    const float keep = persistenceSeconds > 0.f ? std::exp(-elapsedSeconds / persistenceSeconds) : 0.f;
    for (auto& value : intensity)
        value *= keep;

    // Plot the samples that have arrived since the last update
    const auto totalWritten = ringBuffer.getTotalWritten();
    const auto numNew = totalWritten >= lastTotalWritten ? totalWritten - lastTotalWritten : totalWritten;
    const int numSamples = (int)juce::jmin<juce::uint64>(numNew, (juce::uint64)maxSamplesPerUpdate);

    // The trace only carries on from the last update if no samples were left out in between
    if (numNew > (juce::uint64)maxSamplesPerUpdate)
        hasLastPoint = false;

    if (numSamples > 0 && ringBuffer.readLatest(internalBuffer.getWritePointer(0), internalBuffer.getWritePointer(1), numSamples))
    {
        lastTotalWritten = totalWritten;

        // A full-scale mono signal, whose mid is 2, reaches the edge of the circle at a scale of 100%
        const float half = 0.5f * (float)gridSize;
        const float gain = scale * half * 0.5f;
        const auto* left = internalBuffer.getReadPointer(0);
        const auto* right = internalBuffer.getReadPointer(1);

        for (int i = 0; i < numSamples; ++i)
        {
            // Calculate the S and M values for each sample
            float S = (left[i] - right[i]) * gain;
            float M = (left[i] + right[i]) * gain;

            if (!std::isfinite(S) || !std::isfinite(M))
            {
                hasLastPoint = false;
                continue;
            }

            // The polar plot folds the lower half onto the upper half, through the center
            if (mode == polar && M < 0.f)
            {
                S = -S;
                M = -M;
            }

            // The left channel leans to the left, and mid points up
            const juce::Point<float> point { half - S, half - M };

            // The fold breaks the trace up, so the polar plot is made of dots rather than lines
            if (mode == polar || !hasLastPoint)
                addLight(juce::roundToInt(point.x), juce::roundToInt(point.y), dotLight);
            else
                addLine(lastPoint, point);

            lastPoint = point;
            hasLastPoint = true;
        }
    }

    renderImage();
    repaint();
}

void Goniometer::addLine(juce::Point<float> from, juce::Point<float> to)
{
    // The light of one sample is spread along the path of the beam, so a fast beam leaves a dim trace
    const float length = juce::jmax(std::abs(to.x - from.x), std::abs(to.y - from.y));
    const int steps = juce::jlimit(1, 4 * gridSize, (int)std::ceil(length));
    const float amount = lineLight / (float)steps;

    for (int step = 1; step <= steps; ++step)
    {
        const float proportion = (float)step / (float)steps;
        addLight(juce::roundToInt(from.x + (to.x - from.x) * proportion),
                 juce::roundToInt(from.y + (to.y - from.y) * proportion), amount);
    }
}

void Goniometer::addLight(int x, int y, float amount)
{
    if (x >= 0 && y >= 0 && x < gridSize && y < gridSize)
        intensity[(size_t)(y * gridSize + x)] += amount;
}

void Goniometer::renderImage()
{
    juce::Image::BitmapData pixels(image, juce::Image::BitmapData::writeOnly);

    // The brightness saturates as phosphor does, so dense parts of the trace keep their detail
    const float exposure = 0.9f;
    const auto red = traceColour.getRed(), green = traceColour.getGreen(), blue = traceColour.getBlue();

    for (int y = 0; y < gridSize; ++y)
    {
        const float* row = intensity.data() + (size_t)(y * gridSize);
        auto* line = pixels.getLinePointer(y);

        for (int x = 0; x < gridSize; ++x)
        {
            const float brightness = 1.f - std::exp(-exposure * row[x]);

            // The brightest parts of the trace turn from the trace color towards white
            const float whiteness = juce::jmax(0.f, brightness - 0.6f) * 1.5f;
            const auto mix = [&](juce::uint8 channel) { return (juce::uint8)((float)channel + (255.f - (float)channel) * whiteness); };

            // The pixels of an ARGB image hold premultiplied colors
            auto* pixel = (juce::PixelARGB*)(line + x * pixels.pixelStride);
            pixel->setARGB((juce::uint8)(brightness * 255.f), mix(red), mix(green), mix(blue));
            pixel->premultiply();
        }
    }
}

void Goniometer::drawBackground(juce::Graphics& g)
{
    // Draw the background ellipse with the edge color
    g.setColour(edgeColour);
    g.drawEllipse(center.getX() - w / 2, center.getY() - h / 2, w, h, 1);
    // Fill the background ellipse with the base color
    g.setColour(BASE_COLOR);
    g.fillEllipse(center.getX() - w / 2, center.getY() - h / 2, w, h);

    // The axes and their labels: side runs left and right, mid runs up,
    // and the channels lie on the diagonals in between
    const std::array<const char*, 5> labels { "S", "L", "M", "R", "S" };

    // Draw the radial lines and labels
    for (int i = 0; i < 8; ++i)
    {
        // Calculate the end point of each radial line, starting on the left and turning clockwise
        const float angle = (float)i * juce::MathConstants<float>::pi / 4.f - juce::MathConstants<float>::halfPi;
        juce::Point<float> endPoint = center.toFloat().getPointOnCircumference(getRadius(), angle);

        // The polar plot only uses the upper half
        if (mode == polar && i > 4)
            continue;

        // Draw the radial line
        g.setColour(juce::Colours::grey);
        g.drawLine(juce::Line<float>(center.toFloat(), endPoint), 1);

        // Draw the label for each region, just outside the circle
        if (i < (int)labels.size())
        {
            const auto labelCentre = center.toFloat().getPointOnCircumference(getRadius() + 12.f, angle);
            g.setColour(BASE_COLOR);
            g.drawText(labels[(size_t)i],
                juce::Rectangle<float>(20.f, 14.f).withCentre(labelCentre),
                juce::Justification::centred);
        }
    }
}

void Goniometer::updateCoeff(float new_db)
{
    // Update the scaling coefficient with the new dB value
    scale = new_db;
}
