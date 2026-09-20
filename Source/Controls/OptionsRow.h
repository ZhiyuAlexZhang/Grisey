#pragma once

#include <JuceHeader.h>
#include "Buttons.h"

//==============================================================================
// A row of controls that belong to the views. Each control is shown only with the
// views that it was added for, so the row changes as the view does.
class OptionsRow : public juce::Component
{
public:
    using APVTS = juce::AudioProcessorValueTreeState;

    OptionsRow() = default;

    // Builds the bit mask of the views that a control belongs to
    static int views(std::initializer_list<int> viewIds)
    {
        int mask = 0;
        for (int id : viewIds)
            mask |= 1 << id;
        return mask;
    }

    // Adds a combo box for a choice parameter
    void addChoice(int viewMask, APVTS& apvts, const juce::String& parameterID, int width)
    {
        auto box = std::make_unique<juce::ComboBox>();

        // The box needs its items before it is attached to the parameter
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(parameterID)))
            box->addItemList(choice->choices, 1);

        auto& item = addItem(viewMask, width, std::move(box));
        item.comboBoxAttachment = std::make_unique<APVTS::ComboBoxAttachment>(apvts, parameterID, static_cast<juce::ComboBox&>(*item.component));
    }

    // Adds a toggle for a bool parameter
    void addToggle(int viewMask, APVTS& apvts, const juce::String& parameterID, const juce::String& text, int width)
    {
        auto& item = addItem(viewMask, width, std::make_unique<Switch>(text, text));
        item.buttonAttachment = std::make_unique<APVTS::ButtonAttachment>(apvts, parameterID, static_cast<juce::Button&>(*item.component));
    }

    // Adds a button that is not a parameter. If it toggles, onClick can read its state.
    juce::Button& addButton(int viewMask, const juce::String& text, int width, bool toggles, std::function<void()> onClick)
    {
        auto button = std::make_unique<Switch>(text, text);
        button->setClickingTogglesState(toggles);
        button->onClick = std::move(onClick);

        auto& result = *button;
        addItem(viewMask, width, std::move(button));
        return result;
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
            {
                item->component->setBounds(bounds.removeFromLeft(item->width).reduced(0, 2));
                bounds.removeFromLeft(gap);
            }
        }
    }

private:
    static constexpr int gap = 4;

    // The attachments are declared after the component, so that they are destroyed before it
    struct Item
    {
        int viewMask = 0;
        int width = 0;
        std::unique_ptr<juce::Component> component;
        std::unique_ptr<APVTS::ComboBoxAttachment> comboBoxAttachment;
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OptionsRow)
};
