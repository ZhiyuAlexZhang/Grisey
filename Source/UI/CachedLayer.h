#pragma once

#include <JuceHeader.h>

//==============================================================================
// Keeps the part of a display that seldom changes, such as its grid and its labels,
// as an image, so that a frame only has to draw what moves. The image is made at the
// resolution of the screen, and made again when the size, the screen or the content
// changes.
class CachedLayer
{
public:
    // Marks the content as changed
    void invalidate() { dirty = true; }

    // Draws the layer over the bounds, calling paintLayer only if the image has to be made again.
    // paintLayer draws in the coordinates of the bounds, with its origin at their top left.
    template<typename PaintLayer>
    void draw(juce::Graphics& g, juce::Rectangle<int> bounds, bool opaque, PaintLayer&& paintLayer)
    {
        if (bounds.isEmpty())
            return;

        const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
        const int width = juce::roundToInt((float)bounds.getWidth() * scale);
        const int height = juce::roundToInt((float)bounds.getHeight() * scale);

        if (dirty || image.getWidth() != width || image.getHeight() != height)
        {
            image = juce::Image(opaque ? juce::Image::RGB : juce::Image::ARGB, juce::jmax(1, width), juce::jmax(1, height), true);

            juce::Graphics layer(image);
            layer.addTransform(juce::AffineTransform::scale(scale));
            paintLayer(layer);
            dirty = false;
        }

        g.drawImage(image, bounds.toFloat());
    }

private:
    juce::Image image;
    bool dirty = true;
};
