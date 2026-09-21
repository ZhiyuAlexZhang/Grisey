#pragma once

#include <JuceHeader.h>
#include "Theme.h"

//==============================================================================
// The look of the standard JUCE components that the interface uses: the popup
// menus, the rotary knob, and the corner resizer.
class GriseyLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GriseyLookAndFeel()
    {
        setColour(juce::PopupMenu::backgroundColourId, Theme::menu.withAlpha(menuAlpha));
        setColour(juce::PopupMenu::textColourId, Theme::text);
        setColour(juce::PopupMenu::headerTextColourId, Theme::textDim);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, Theme::menuHighlight);
        setColour(juce::PopupMenu::highlightedTextColourId, Theme::accent);
        setColour(juce::ResizableWindow::backgroundColourId, Theme::window);
    }

    juce::Font getPopupMenuFont() override { return Theme::controlFont(); }

    // The menus are in the manner of FabFilter's: a panel with rounded corners and a hairline edge, rows
    // that are close together, a solid blue bar under the row that the mouse is on, the current choice in
    // the accent color with a dot, and a space, not a line, between groups. The background color that is
    // registered for menus is not quite opaque, which is what makes JUCE give the menu a window that can
    // have rounded corners.
    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override
    {
        const auto bounds = juce::Rectangle<float>(0.f, 0.f, (float)width, (float)height);
        g.setColour(Theme::menu.withAlpha(menuAlpha));
        g.fillRoundedRectangle(bounds, menuCornerRadius);
        g.setColour(Theme::panelEdge);
        g.drawRoundedRectangle(bounds.reduced(0.5f), menuCornerRadius, 1.f);
    }

    int getPopupMenuBorderSize() override { return 5; }

    // An item of a menu, from the left: room for the dot of the current choice, the text, and room
    // for the arrow of a submenu
    void getIdealPopupMenuItemSize(const juce::String& text, bool isSeparator, int standardMenuItemHeight,
                                   int& idealWidth, int& idealHeight) override
    {
        idealHeight = isSeparator ? menuGroupGap : standardMenuItemHeight > 0 ? standardMenuItemHeight : menuItemHeight;
        idealWidth = isSeparator ? 50 : Theme::textWidth(getPopupMenuFont(), text) + 2 * menuInset + menuMarkWidth + menuArrowWidth;
    }

    // The name of a section is lower than an item, where JUCE would make it half as high again
    void getIdealPopupMenuSectionHeaderSizeWithOptions(const juce::String& text, int, int& idealWidth, int& idealHeight,
                                                       const juce::PopupMenu::Options&) override
    {
        idealHeight = menuSectionNameHeight;
        idealWidth = Theme::textWidth(Theme::labelFont(), text.toUpperCase()) + 2 * menuInset + menuMarkWidth;
    }

    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                           bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                           const juce::String&, const juce::Drawable*, const juce::Colour*) override
    {
        // A separator is only a space between two groups
        if (isSeparator)
            return;

        const bool isLit = isHighlighted && isActive;
        if (isLit)
        {
            g.setColour(Theme::menuHighlight);
            g.fillRect(area);
        }

        auto bounds = area.reduced(menuInset, 0);

        // The current choice has a dot in the accent color, as the toggles of the bar have
        const auto mark = bounds.removeFromLeft(menuMarkWidth).toFloat();
        if (isTicked)
        {
            g.setColour(Theme::accent);
            g.fillEllipse(juce::Rectangle<float>(4.5f, 4.5f).withCentre({ mark.getX() + 3.f, mark.getCentreY() }));
        }

        // A submenu has a thin chevron at the right
        const auto arrow = bounds.removeFromRight(menuArrowWidth).toFloat();
        if (hasSubMenu)
        {
            juce::Path chevron;
            const auto tip = juce::Point<float>(arrow.getRight() - 1.f, arrow.getCentreY());
            chevron.startNewSubPath(tip.x - 3.f, tip.y - 3.f);
            chevron.lineTo(tip);
            chevron.lineTo(tip.x - 3.f, tip.y + 3.f);
            g.setColour(isLit ? juce::Colours::white : Theme::textDim);
            g.strokePath(chevron, juce::PathStrokeType(1.2f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
        }

        g.setFont(getPopupMenuFont());
        g.setColour(!isActive ? Theme::textFaint : isLit ? juce::Colours::white : isTicked ? Theme::accent : Theme::text);
        g.drawText(text, bounds, juce::Justification::centredLeft);
    }

    // The name of a section is a small label close above its items, in line with their text
    void drawPopupMenuSectionHeader(juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& sectionName) override
    {
        g.setFont(Theme::labelFont());
        g.setColour(Theme::textDim);
        g.drawText(sectionName.toUpperCase(), area.reduced(menuInset, 0).withTrimmedLeft(menuMarkWidth).withTrimmedBottom(2),
                   juce::Justification::bottomLeft);
    }

    // A dark knob inside a blue ring, with a ring of dots for its scale, and a white lens at its rim for its pointer
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float position,
                          float startAngle, float endAngle, juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(2.f);
        const float radius = 0.5f * juce::jmin(bounds.getWidth(), bounds.getHeight());
        const auto centre = bounds.getCentre();
        const float angle = startAngle + position * (endAngle - startAngle);

        // The dots of the scale, lit up to the value
        const int numDots = 11;
        for (int dot = 0; dot < numDots; ++dot)
        {
            const float proportion = (float)dot / (float)(numDots - 1);
            const auto point = centre.getPointOnCircumference(radius - 1.5f, startAngle + proportion * (endAngle - startAngle));
            g.setColour(proportion <= position + 0.001f && slider.isEnabled() ? Theme::second : Theme::secondDeep);
            g.fillEllipse(juce::Rectangle<float>(2.6f, 2.6f).withCentre(point));
        }

        // The ring, and the body within it, which is lit from above
        const float ringRadius = radius - 7.f;
        const float bodyRadius = ringRadius - 2.5f;
        const auto body = juce::Rectangle<float>(2.f * bodyRadius, 2.f * bodyRadius).withCentre(centre);

        g.setColour(Theme::secondDeep);
        g.drawEllipse(juce::Rectangle<float>(2.f * ringRadius, 2.f * ringRadius).withCentre(centre), 2.f);

        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff2c2d3c), centre.x, body.getY(), juce::Colour(0xff171d35), centre.x, body.getBottom(), false));
        g.fillEllipse(body);
        g.setColour(juce::Colour(0xff131116));
        g.drawEllipse(body, 1.f);

        // The pointer is an ellipse that the edge of the body cuts into a lens
        {
            juce::Graphics::ScopedSaveState state(g);

            juce::Path bodyShape;
            bodyShape.addEllipse(body.reduced(1.f));
            g.reduceClipRegion(bodyShape);

            const auto toAngle = juce::AffineTransform::rotation(angle, centre.x, centre.y);
            const auto lensCentre = juce::Point<float>(centre.x, centre.y - bodyRadius * 0.82f);

            juce::Path lens;
            lens.addEllipse(juce::Rectangle<float>(bodyRadius * 0.84f, bodyRadius * 0.9f).withCentre(lensCentre));

            const auto lit = lensCentre.transformedBy(toAngle);
            g.setGradientFill(juce::ColourGradient(juce::Colours::white, lit.x, lit.y, juce::Colour(0xffbfbfbf), lit.x + bodyRadius * 0.5f, lit.y + bodyRadius * 0.5f, true));
            g.fillPath(lens, toAngle);

            juce::Path tick;
            tick.addRoundedRectangle(centre.x - 0.75f, centre.y - bodyRadius * 0.86f, 1.5f, bodyRadius * 0.24f, 0.75f);
            g.setColour(juce::Colour(0xff131016));
            g.fillPath(tick, toAngle);
        }
    }

    void drawCornerResizer(juce::Graphics& g, int width, int height, bool, bool isMouseOver) override
    {
        g.setColour(isMouseOver ? Theme::textDim : Theme::textFaint);
        for (float inset : { 0.35f, 0.6f, 0.85f })
            g.drawLine((float)width * inset, (float)height - 3.f, (float)width - 3.f, (float)height * inset, 1.f);
    }

private:
    // The measures of a menu
    static constexpr int menuItemHeight = 22;
    static constexpr int menuSectionNameHeight = 21;
    static constexpr int menuGroupGap = 8;
    static constexpr int menuInset = 10;
    static constexpr int menuMarkWidth = 12;
    static constexpr int menuArrowWidth = 16;
    static constexpr float menuCornerRadius = 5.f;
    static constexpr float menuAlpha = 0.97f;
};
