#pragma once

#include <JuceHeader.h>

//==============================================================================
// An image that scrolls from right to left as columns are added at its right edge. The columns
// are written round and round the image, and draw() shows it in two parts, so that adding a
// column never moves the pixels that are already there. A frame costs one column.
class ScrollingImage
{
public:
    // Makes the image again at a new size, and clears it to a color
    void setSize(int width, int height, juce::Colour background)
    {
        image = juce::Image(juce::Image::RGB, juce::jmax(1, width), juce::jmax(1, height), false);
        clear(background);
    }

    void clear(juce::Colour background)
    {
        if (image.isValid())
        {
            juce::Graphics g(image);
            g.fillAll(background);
        }

        writeX = 0;
    }

    int getWidth() const { return image.getWidth(); }
    int getHeight() const { return image.getHeight(); }

    // Adds a column at the right edge. colourOfRow is called for every row, from the top down.
    template<typename ColourOfRow>
    void addColumn(ColourOfRow&& colourOfRow)
    {
        if (!image.isValid())
            return;

        {
            juce::Image::BitmapData pixels(image, writeX, 0, 1, image.getHeight(), juce::Image::BitmapData::writeOnly);
            for (int row = 0; row < image.getHeight(); ++row)
                pixels.setPixelColour(0, row, colourOfRow(row));
        }

        writeX = (writeX + 1) % image.getWidth();
    }

    // Draws the image over an area, with the newest column at the right
    void draw(juce::Graphics& g, juce::Rectangle<int> area) const
    {
        if (!image.isValid())
            return;

        // The oldest columns start at writeX, and the newest end just before it. The image has one
        // pixel per point of the area, so the parts keep their proportions if the scale is not 1.
        const int width = image.getWidth(), height = image.getHeight();
        const float scaleX = (float)area.getWidth() / (float)width;
        const int split = juce::roundToInt((float)(width - writeX) * scaleX);

        // A column is wider than a pixel when the span is short, and would show as a block without this
        g.setImageResamplingQuality(juce::Graphics::mediumResamplingQuality);
        g.drawImage(image, area.getX(), area.getY(), split, area.getHeight(), writeX, 0, width - writeX, height);

        if (writeX > 0)
            g.drawImage(image, area.getX() + split, area.getY(), area.getWidth() - split, area.getHeight(), 0, 0, writeX, height);
    }

private:
    juce::Image image;
    int writeX = 0;
};
