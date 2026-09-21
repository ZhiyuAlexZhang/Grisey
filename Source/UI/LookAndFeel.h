#pragma once

#include <JuceHeader.h>
#include "Theme.h"

//==============================================================================
// The look of the standard JUCE components that the interface uses: the popup
// menus, the rotary knob, and the corner resizer.
class MultiMeterLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MultiMeterLookAndFeel()
    {
        setColour(juce::PopupMenu::backgroundColourId, Theme::menu);
        setColour(juce::PopupMenu::textColourId, Theme::text);
        setColour(juce::PopupMenu::headerTextColourId, Theme::textDim);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, Theme::menuHighlight);
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour(juce::ResizableWindow::backgroundColourId, Theme::window);
    }

    juce::Font getPopupMenuFont() override { return Theme::controlFont(); }

    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override
    {
        g.fillAll(Theme::menu);
        g.setColour(Theme::gridStrong);
        g.drawRect(0, 0, width, height, 1);
    }

    // A knob whose value is an arc around it, with a pointer on the cap
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float position,
                          float startAngle, float endAngle, juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.f);
        const float radius = 0.5f * juce::jmin(bounds.getWidth(), bounds.getHeight());
        const auto centre = bounds.getCentre();
        const float angle = startAngle + position * (endAngle - startAngle);
        const float arcRadius = radius - 2.f;

        juce::Path track, value;
        track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.f, startAngle, endAngle, true);
        value.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.f, startAngle, angle, true);

        const auto stroke = juce::PathStrokeType(3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        g.setColour(Theme::gridStrong);
        g.strokePath(track, stroke);
        g.setColour(slider.isEnabled() ? Theme::accent : Theme::textFaint);
        g.strokePath(value, stroke);

        // The cap is lit from above
        const float capRadius = radius - 8.f;
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff2c3548), centre.x, centre.y - capRadius,
                                               juce::Colour(0xff171c28), centre.x, centre.y + capRadius, false));
        g.fillEllipse(juce::Rectangle<float>(2.f * capRadius, 2.f * capRadius).withCentre(centre));
        g.setColour(juce::Colour(0xff3a455c));
        g.drawEllipse(juce::Rectangle<float>(2.f * capRadius, 2.f * capRadius).withCentre(centre), 1.f);

        juce::Path pointer;
        pointer.addRoundedRectangle(-1.25f, -capRadius + 3.f, 2.5f, capRadius * 0.45f, 1.25f);
        g.setColour(Theme::text);
        g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre));
    }

    void drawCornerResizer(juce::Graphics& g, int width, int height, bool, bool isMouseOver) override
    {
        g.setColour(isMouseOver ? Theme::textDim : Theme::textFaint);
        for (float inset : { 0.35f, 0.6f, 0.85f })
            g.drawLine((float)width * inset, (float)height - 3.f, (float)width - 3.f, (float)height * inset, 1.f);
    }
};
