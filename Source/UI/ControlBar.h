#pragma once

#include <JuceHeader.h>
#include "Controls.h"

//==============================================================================
// The controls of the views, in a row along the bottom of the window. Each control is
// shown only with the views that it was added for, so the row changes as the view does.
class ControlBar : public juce::Component
{
public:
    using APVTS = juce::AudioProcessorValueTreeState;

    ControlBar() = default;

    // Builds the bit mask of the views that a control belongs to
    static int views(std::initializer_list<int> viewIds)
    {
        int mask = 0;
        for (int id : viewIds)
            mask |= 1 << id;
        return mask;
    }

    // Adds a menu for a choice parameter
    void addMenu(int viewMask, APVTS& apvts, const juce::String& parameterID, const juce::String& prefix = {})
    {
        auto menu = std::make_unique<MenuButton>(*apvts.getParameter(parameterID), prefix);
        const int width = menu->getIdealWidth();
        addItem(viewMask, width, std::move(menu));
    }

    // Adds a toggle for a bool parameter
    void addToggle(int viewMask, APVTS& apvts, const juce::String& parameterID, const juce::String& text)
    {
        auto button = std::make_unique<TextButtonQuiet>(text);
        button->setClickingTogglesState(true);
        const int width = button->getIdealWidth();

        auto& item = addItem(viewMask, width, std::move(button));
        item.buttonAttachment = std::make_unique<APVTS::ButtonAttachment>(apvts, parameterID, static_cast<juce::Button&>(*item.component));
    }

    // Adds a button that is not a parameter. If it toggles, onClick can read its state.
    juce::Button& addButton(int viewMask, const juce::String& text, bool toggles, std::function<void()> onClick)
    {
        auto button = std::make_unique<TextButtonQuiet>(text);
        button->setClickingTogglesState(toggles);
        button->onClick = std::move(onClick);
        const int width = button->getIdealWidth();

        auto& result = *button;
        addItem(viewMask, width, std::move(button));
        return result;
    }

    // The width that the controls of the current view take up, from the left
    int getUsedWidth() const
    {
        int width = 0;
        for (auto& item : items)
            if ((item->viewMask & (1 << currentView)) != 0)
                width += item->width;
        return width;
    }

    // Shows the controls of a view, and hides the rest
    void showView(int viewId)
    {
        currentView = viewId;
        resized();
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        for (auto& item : items)
        {
            const bool visible = (item->viewMask & (1 << currentView)) != 0;
            item->component->setVisible(visible);

            if (visible)
                item->component->setBounds(bounds.removeFromLeft(item->width));
        }
    }

private:
    // The attachment is declared after the component, so that it is destroyed before it
    struct Item
    {
        int viewMask = 0;
        int width = 0;
        std::unique_ptr<juce::Component> component;
        std::unique_ptr<APVTS::ButtonAttachment> buttonAttachment;
    };

    Item& addItem(int viewMask, int width, std::unique_ptr<juce::Component> component)
    {
        auto item = std::make_unique<Item>();
        item->viewMask = viewMask;
        item->width = width;
        item->component = std::move(component);
        addChildComponent(*item->component);

        items.push_back(std::move(item));
        return *items.back();
    }

    std::vector<std::unique_ptr<Item>> items;
    int currentView = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ControlBar)
};
