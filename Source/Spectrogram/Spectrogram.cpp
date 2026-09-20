#include "Spectrogram.h"

//==============================================================================
// Implementation for the Spectrogram class
Spectrogram::Spectrogram(SpectrumSource& spectrumSource) : source(spectrumSource)
{
    // From black through purple, red and orange to pale yellow, so that the lightness rises with the level
    juce::ColourGradient gradient;
    gradient.addColour(0.0, juce::Colour(0xff000004));
    gradient.addColour(0.25, juce::Colour(0xff3b0f70));
    gradient.addColour(0.5, juce::Colour(0xffb73779));
    gradient.addColour(0.75, juce::Colour(0xfffc8961));
    gradient.addColour(1.0, juce::Colour(0xfffcfdbf));

    for (size_t i = 0; i < colourTable.size(); ++i)
        colourTable[i] = gradient.getColourAtPosition((double)i / (double)(colourTable.size() - 1));
}

void Spectrogram::paint(juce::Graphics& g)
{
    // Fill the background with a rounded rectangle using the base color
    g.setColour(BASE_COLOR);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 4);

    const auto area = getImageArea();

    if (image.isValid())
    {
        // The oldest columns start at writeX, and the newest end just before it
        const int width = image.getWidth(), height = image.getHeight();
        g.drawImage(image, area.getX(), area.getY(), width - writeX, height, writeX, 0, width - writeX, height);
        g.drawImage(image, area.getX() + width - writeX, area.getY(), writeX, height, 0, 0, writeX, height);
    }

    // Mark the decades of the frequency axis
    g.setFont(12.f);
    for (const auto& [frequency, text] : { std::pair<double, const char*> { 100.0, "100" }, { 1000.0, "1k" }, { 10000.0, "10k" } })
    {
        const double proportion = std::log(frequency / display.minFrequency) / std::log(display.maxFrequency / display.minFrequency);
        const int y = area.getBottom() - juce::roundToInt(proportion * area.getHeight());

        g.setColour(juce::Colours::white.withAlpha(0.25f));
        g.drawHorizontalLine(y, (float)area.getX(), (float)area.getRight());
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.drawText(text, area.getX() + 4, y - 14, 40, 14, juce::Justification::centredLeft);
    }
}

void Spectrogram::resized()
{
    const auto area = getImageArea();
    if (area.isEmpty())
        return;

    // One display point for every row of the image
    image = juce::Image(juce::Image::RGB, area.getWidth(), area.getHeight(), true);
    display.numPoints = area.getHeight();
    writeX = 0;
}

void Spectrogram::mouseDown(const juce::MouseEvent&)
{
    if (image.isValid())
        image.clear(image.getBounds());

    repaint();
}

void Spectrogram::addColumn(float tiltDbPerOctave)
{
    if (!image.isValid())
        return;

    display.tiltDbPerOctave = tiltDbPerOctave;
    source.getEngine().render(SpectrumEngine::Curve::mid, display, source.getSampleRate(), column);

    {
        // The first display point is the lowest frequency, which belongs at the bottom of the image
        juce::Image::BitmapData pixels(image, writeX, 0, 1, image.getHeight(), juce::Image::BitmapData::writeOnly);
        const int height = image.getHeight();
        for (int row = 0; row < height; ++row)
            pixels.setPixelColour(0, row, colourFor(column[(size_t)(height - 1 - row)]));
    }

    writeX = (writeX + 1) % image.getWidth();
    repaint();
}

juce::Rectangle<int> Spectrogram::getImageArea() const
{
    return getLocalBounds().reduced(8);
}

juce::Colour Spectrogram::colourFor(float decibels) const
{
    const float proportion = juce::jlimit(0.f, 1.f, (decibels - minDecibels) / (maxDecibels - minDecibels));
    return colourTable[(size_t)juce::roundToInt(proportion * (float)(colourTable.size() - 1))];
}
