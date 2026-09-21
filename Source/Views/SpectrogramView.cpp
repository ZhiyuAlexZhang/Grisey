#include "SpectrogramView.h"

//==============================================================================
SpectrogramView::SpectrogramView(SpectrumSource& spectrumSource) : source(spectrumSource)
{
    setOpaque(true);

    // From the black of the display through the deep and the bright blue to the yellow of the signal
    // and on to white, so that the lightness rises with the level
    juce::ColourGradient gradient;
    gradient.addColour(0.0, Theme::displayTop);
    gradient.addColour(0.3, Theme::secondDeep);
    gradient.addColour(0.55, Theme::second);
    gradient.addColour(0.82, Theme::accent);
    gradient.addColour(1.0, juce::Colours::white);

    for (size_t i = 0; i < colourTable.size(); ++i)
        colourTable[i] = gradient.getColourAtPosition((double)i / (double)(colourTable.size() - 1));
}

float SpectrogramView::yOf(double frequency) const
{
    const double proportion = std::log(frequency / display.minFrequency) / std::log(display.maxFrequency / display.minFrequency);
    return (float)plot.getBottom() - (float)proportion * (float)plot.getHeight();
}

void SpectrogramView::resized()
{
    plot = getLocalBounds().withTrimmedLeft(34).withTrimmedTop(8).withTrimmedBottom(8).withTrimmedRight(8);

    // One display point for every row of the image
    image.setSize(plot.getWidth(), plot.getHeight(), Theme::displayTop);
    display.numPoints = juce::jmax(2, plot.getHeight());
}

void SpectrogramView::paint(juce::Graphics& g)
{
    Theme::fillDisplay(g, getLocalBounds());
    image.draw(g, plot);

    // Mark the frequency axis, stronger at the decades
    g.setFont(Theme::font(10.5f));
    for (const auto& [frequency, text] : { std::pair<double, const char*> { 50.0, "50" }, { 100.0, "100" }, { 200.0, "200" }, { 500.0, "500" },
                                          { 1000.0, "1k" }, { 2000.0, "2k" }, { 5000.0, "5k" }, { 10000.0, "10k" } })
    {
        const bool isDecade = juce::exactlyEqual(frequency, 100.0) || juce::exactlyEqual(frequency, 1000.0) || juce::exactlyEqual(frequency, 10000.0);
        const float y = yOf(frequency);

        g.setColour(juce::Colours::white.withAlpha(isDecade ? 0.14f : 0.06f));
        g.fillRect((float)plot.getX(), y, (float)plot.getWidth(), 1.f);
        g.setColour(isDecade ? Theme::textDim : Theme::textFaint);
        g.drawText(text, juce::Rectangle<float>(0.f, y - 7.f, (float)plot.getX() - 6.f, 14.f), juce::Justification::centredRight);
    }

    if (hoverY.has_value())
    {
        const double proportion = (double)(plot.getBottom() - *hoverY) / (double)juce::jmax(1, plot.getHeight());
        const double frequency = display.minFrequency * std::pow(display.maxFrequency / display.minFrequency, proportion);

        g.setColour(juce::Colours::white.withAlpha(0.4f));
        g.fillRect((float)plot.getX(), (float)*hoverY, (float)plot.getWidth(), 1.f);

        const auto text = Theme::formatFrequency(frequency) + "    " + Theme::formatNote(frequency);
        auto box = juce::Rectangle<int>(Theme::textWidth(Theme::controlFont(), text) + 20, 24)
                       .withPosition(plot.getX() + 10, juce::jlimit(plot.getY(), plot.getBottom() - 24, *hoverY - 30));

        Theme::drawPanel(g, box.toFloat());
        g.setFont(Theme::controlFont());
        g.setColour(Theme::text);
        g.drawText(text, box, juce::Justification::centred);
    }
}

void SpectrogramView::addColumn(float tiltDbPerOctave)
{
    display.tiltDbPerOctave = tiltDbPerOctave;
    source.getEngine().render(SpectrumEngine::Curve::mid, display, source.getSampleRate(), column);

    // The first display point is the lowest frequency, which belongs at the bottom of the image
    const int height = image.getHeight();
    image.addColumn([&](int row)
    {
        const float decibels = column[(size_t)juce::jlimit(0, (int)column.size() - 1, height - 1 - row)];
        const float proportion = juce::jlimit(0.f, 1.f, (decibels - minDecibels) / (maxDecibels - minDecibels));
        return colourTable[(size_t)juce::roundToInt(proportion * (float)(colourTable.size() - 1))];
    });

    repaint(plot);
}

void SpectrogramView::mouseMove(const juce::MouseEvent& e)
{
    const std::optional<int> newHover = plot.contains(e.getPosition()) ? std::optional(e.getPosition().y) : std::nullopt;
    if (newHover != hoverY)
    {
        hoverY = newHover;
        repaint();
    }
}

void SpectrogramView::mouseExit(const juce::MouseEvent&)
{
    if (hoverY.has_value())
    {
        hoverY.reset();
        repaint();
    }
}

void SpectrogramView::mouseDown(const juce::MouseEvent&)
{
    image.clear(Theme::displayTop);
    repaint();
}
