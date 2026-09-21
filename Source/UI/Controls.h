#pragma once

#include <JuceHeader.h>
#include "Theme.h"

//==============================================================================
// The controls of the header and the bottom bar. They are quiet pieces of text that
// brighten under the mouse, so that the displays stay the loudest thing on the screen.

//==============================================================================
// Adds the options of a choice parameter to a menu, with a tick beside the current one.
// Choosing an option sets the parameter as one gesture, so that a host can undo it.
inline void addChoiceItems(juce::PopupMenu& menu, juce::RangedAudioParameter& parameter)
{
    const auto& names = parameter.getAllValueStrings();
    const int current = juce::roundToInt(parameter.convertFrom0to1(parameter.getValue()));

    for (int index = 0; index < names.size(); ++index)
    {
        menu.addItem(names[index], true, index == current, [&parameter, index]
        {
            parameter.beginChangeGesture();
            parameter.setValueNotifyingHost(parameter.convertTo0to1((float)index));
            parameter.endChangeGesture();
        });
    }
}

//==============================================================================
// A choice parameter as its current option and a chevron, which opens a menu of the options.
// An optional prefix names the setting, for options that do not explain themselves.
class MenuButton : public juce::Component
{
public:
    explicit MenuButton(juce::RangedAudioParameter& parameterToControl, juce::String prefixText = {}) :
        parameter(parameterToControl),
        prefix(std::move(prefixText)),
        attachment(parameterToControl, [this](float value) { setCurrent(juce::roundToInt(value)); })
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        attachment.sendInitialUpdate();
    }

    // The width that the widest option needs, so that the bar does not shift as the option changes
    int getIdealWidth() const
    {
        int widest = 0;
        for (auto& name : parameter.getAllValueStrings())
            widest = juce::jmax(widest, Theme::textWidth(Theme::controlFont(), name));

        const int prefixWidth = prefix.isEmpty() ? 0 : Theme::textWidth(Theme::labelFont(), prefix) + 6;
        return prefixWidth + widest + chevronWidth + 2 * padding;
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().reduced(padding, 0);

        if (prefix.isNotEmpty())
        {
            g.setFont(Theme::labelFont());
            g.setColour(Theme::textFaint.brighter(isMouseOver() ? 0.4f : 0.f));
            g.drawText(prefix, bounds.removeFromLeft(Theme::textWidth(Theme::labelFont(), prefix) + 6), juce::Justification::centredLeft);
        }

        // The chevron follows the text, however short the current option is
        const juce::String text = parameter.getAllValueStrings()[current];
        const auto textArea = bounds.removeFromLeft(Theme::textWidth(Theme::controlFont(), text) + 2);
        const auto chevronArea = bounds.removeFromLeft(chevronWidth).toFloat();

        g.setFont(Theme::controlFont());
        g.setColour(isMouseOver() ? Theme::text : Theme::textDim);
        g.drawText(text, textArea, juce::Justification::centredLeft);

        juce::Path chevron;
        const auto centre = chevronArea.getCentre();
        chevron.startNewSubPath(centre.x - 3.f, centre.y - 1.5f);
        chevron.lineTo(centre.x, centre.y + 1.5f);
        chevron.lineTo(centre.x + 3.f, centre.y - 1.5f);
        g.strokePath(chevron, juce::PathStrokeType(1.2f));
    }

    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }

    void mouseDown(const juce::MouseEvent&) override
    {
        juce::PopupMenu menu;
        addChoiceItems(menu, parameter);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withPreferredPopupDirection(juce::PopupMenu::Options::PopupDirection::upwards));
    }

private:
    static constexpr int padding = 8;
    static constexpr int chevronWidth = 14;

    void setCurrent(int index)
    {
        current = juce::jlimit(0, parameter.getAllValueStrings().size() - 1, index);
        repaint();
    }

    juce::RangedAudioParameter& parameter;
    juce::String prefix;
    int current = 0;
    juce::ParameterAttachment attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MenuButton)
};

//==============================================================================
// A button that is a piece of text. When it toggles, a dot and the accent color show that it is on.
class TextButtonQuiet : public juce::Button
{
public:
    explicit TextButtonQuiet(const juce::String& text) : juce::Button(text)
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    int getIdealWidth() const
    {
        return Theme::textWidth(Theme::controlFont(), getName()) + 2 * padding + (isToggleable() ? dotWidth : 0);
    }

    void paintButton(juce::Graphics& g, bool isHighlighted, bool isDown) override
    {
        auto bounds = getLocalBounds().reduced(padding, 0);
        const bool on = getToggleState();
        const auto colour = on ? Theme::accent : (isHighlighted || isDown) ? Theme::text : Theme::textDim;

        if (isToggleable())
        {
            const auto dot = bounds.removeFromLeft(dotWidth).toFloat().withSizeKeepingCentre(5.f, 5.f).translated(-2.f, 0.f);
            g.setColour(on ? Theme::accent : Theme::textFaint);
            if (on)
                g.fillEllipse(dot);
            else
                g.drawEllipse(dot, 1.f);
        }

        g.setFont(Theme::controlFont());
        g.setColour(colour);
        g.drawText(getName(), bounds, juce::Justification::centredLeft);
    }

private:
    static constexpr int padding = 8;
    static constexpr int dotWidth = 12;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TextButtonQuiet)
};

//==============================================================================
// A button that opens a menu which is built when it is clicked, for settings that are
// too many to each have a place on the bar.
class SettingsButton : public juce::Component
{
public:
    explicit SettingsButton(const juce::String& text) : label(text)
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    // Fills the menu, called every time that the button is clicked
    std::function<void(juce::PopupMenu&)> buildMenu;

    int getIdealWidth() const { return Theme::textWidth(Theme::controlFont(), label) + 38; }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().reduced(8, 0);
        const auto colour = isMouseOver() ? Theme::text : Theme::textDim;

        // Three sliders, as a sign for settings
        auto icon = bounds.removeFromLeft(16).toFloat().withSizeKeepingCentre(11.f, 9.f);
        g.setColour(colour);
        for (int line = 0; line < 3; ++line)
        {
            const float y = icon.getY() + 1.f + (float)line * 3.5f;
            g.fillRect(icon.getX(), y, icon.getWidth(), 1.f);
            g.fillRect(icon.getX() + (line == 1 ? 6.f : 2.f), y - 1.f, 2.f, 3.f);
        }

        g.setFont(Theme::controlFont());
        g.drawText(label, bounds.withTrimmedLeft(4), juce::Justification::centredLeft);
    }

    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }

    void mouseDown(const juce::MouseEvent&) override
    {
        juce::PopupMenu menu;
        if (buildMenu)
            buildMenu(menu);

        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withPreferredPopupDirection(juce::PopupMenu::Options::PopupDirection::upwards));
    }

private:
    juce::String label;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsButton)
};

//==============================================================================
// The tabs of the header, one for each view. The tab of the current view is bright and underlined.
class TabBar : public juce::Component
{
public:
    TabBar() = default;

    // Called with the index of the tab that the user has clicked
    std::function<void(int)> onChange;

    void setTabs(const juce::StringArray& tabNames)
    {
        names.clear();
        for (auto& name : tabNames)
            names.add(name.toUpperCase());

        resized();
    }

    // Shows a tab as the current one, without calling onChange
    void setSelection(int index)
    {
        if (selected != index)
        {
            selected = index;
            repaint();
        }
    }

    int getIdealWidth() const
    {
        int width = 0;
        for (auto& name : names)
            width += Theme::textWidth(tabFont(), name) + 2 * padding;
        return width;
    }

    void resized() override
    {
        areas.clear();
        auto bounds = getLocalBounds();
        for (auto& name : names)
            areas.add(bounds.removeFromLeft(Theme::textWidth(tabFont(), name) + 2 * padding));
    }

    void paint(juce::Graphics& g) override
    {
        g.setFont(tabFont());

        for (int index = 0; index < names.size(); ++index)
        {
            const bool isSelected = index == selected;
            g.setColour(isSelected ? Theme::text : index == hovered ? Theme::textDim.brighter(0.4f) : Theme::textDim);
            g.drawText(names[index], areas[index], juce::Justification::centred);

            if (isSelected)
            {
                g.setColour(Theme::accent);
                g.fillRect(areas[index].reduced(padding, 0).removeFromBottom(2));
            }
        }
    }

    void mouseMove(const juce::MouseEvent& e) override { setHovered(tabAt(e.getPosition())); }
    void mouseExit(const juce::MouseEvent&) override { setHovered(-1); }

    void mouseDown(const juce::MouseEvent& e) override
    {
        const int index = tabAt(e.getPosition());
        if (index >= 0 && index != selected)
        {
            setSelection(index);
            if (onChange)
                onChange(index);
        }
    }

private:
    static constexpr int padding = 12;
    static juce::Font tabFont() { return Theme::font(11.5f, true); }

    int tabAt(juce::Point<int> position) const
    {
        for (int index = 0; index < areas.size(); ++index)
            if (areas[index].contains(position))
                return index;
        return -1;
    }

    void setHovered(int index)
    {
        if (hovered != index)
        {
            hovered = index;
            setMouseCursor(index >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
            repaint();
        }
    }

    juce::StringArray names;
    juce::Array<juce::Rectangle<int>> areas;
    int selected = 0, hovered = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TabBar)
};

//==============================================================================
// A rotary knob with its name above it and its value below, which floats over a display.
// It is faint until the mouse is over the display, so that it does not distract.
class FloatingKnob : public juce::Component
{
public:
    FloatingKnob(juce::AudioProcessorValueTreeState& apvts, const juce::String& parameterID, const juce::String& nameText, const juce::String& suffixText) :
        name(nameText), suffix(suffixText)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        slider.onValueChange = [this] { repaint(); };
        addAndMakeVisible(slider);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, parameterID, slider);
        setAlpha(restingAlpha);
    }

    // Brightens the knob while the mouse is over the display that it floats on
    void setEmphasized(bool shouldBeEmphasized)
    {
        const float alpha = shouldBeEmphasized ? 1.f : restingAlpha;
        if (!juce::exactlyEqual(alpha, getAlpha()))
            juce::Desktop::getInstance().getAnimator().animateComponent(this, getBounds(), alpha, 160, false, 1.0, 1.0);
    }

    void resized() override
    {
        slider.setBounds(getLocalBounds().withTrimmedTop(14).withTrimmedBottom(14));
    }

    void paint(juce::Graphics& g) override
    {
        g.setFont(Theme::labelFont());
        g.setColour(Theme::textDim);
        g.drawText(name.toUpperCase(), getLocalBounds().removeFromTop(14), juce::Justification::centred);
        g.setColour(Theme::text);
        g.drawText(juce::String(juce::roundToInt(slider.getValue())) + suffix, getLocalBounds().removeFromBottom(14), juce::Justification::centred);
    }

private:
    static constexpr float restingAlpha = 0.45f;

    juce::String name, suffix;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FloatingKnob)
};
