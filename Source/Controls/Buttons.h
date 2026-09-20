
#pragma once

#include <JuceHeader.h>
using namespace juce;

//==============================================================================
// This class defines a custom LookAndFeel for buttons, including ToggleButtons and TextButtons.
class ButtonsLook : public LookAndFeel_V4
{
public:
    // Constructor
    ButtonsLook() {}

    // Destructor
    ~ButtonsLook() override {}

    // Override the drawButtonBackground method to customize the appearance of buttons.
    void drawButtonBackground(Graphics& g, Button& button,
        const Colour& backgroundColour,
        bool shouldDrawButtonAsHighlighted,
        bool shouldDrawButtonAsDown) override
    {
        // Get the button area and corner size
        auto buttonArea = button.getLocalBounds().toFloat();
        float cornerSize = 3.0f;

        // Get the button color
        auto buttonColor = button.findColour(TextButton::buttonColourId);

        // Set the color based on the toggle state and fill the rounded rectangle
        g.setColour(button.getToggleState() ? buttonColor.darker(0.5) : buttonColor);
        g.fillRoundedRectangle(buttonArea, cornerSize);
    }

    // Override the getComboBoxFont method to customize the font of ComboBoxes.
    Font getComboBoxFont(ComboBox& combobox) override
    {
        auto font = getPopupMenuFont();
        font.setHeight(14);
        return font;
    }

    // Override the drawToggleButton method to customize the appearance of ToggleButtons.
    void drawToggleButton(Graphics& g, ToggleButton& button, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        // Get the button color
        Colour buttonColor = button.findColour(TextButton::buttonColourId);

        // Set the color based on the toggle state
        if (button.getToggleState())
            g.setColour(buttonColor);
        else
            g.setColour(buttonColor.darker(0.5f));

        // Draw the button background
        drawButtonBackground(g, button, buttonColor, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

        // Set the text color and draw the button text centered within the button area
        g.setColour(button.findColour(ToggleButton::textColourId));
        auto buttonArea = button.getLocalBounds();
        g.drawFittedText(button.getButtonText(), buttonArea, Justification::centred, 1);
    }

    // Override the getTextButtonFont method to customize the font of TextButtons.
    Font getTextButtonFont(TextButton&, int buttonHeight) override
    {
        return Font("Arial", buttonHeight * 0.8f, Font::plain);
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ButtonsLook)
};

//==============================================================================
// This class defines a custom LookAndFeel for ToggleButtons, which changes the text color and background color
class CustomLook : public LookAndFeel_V4
{
public:
    CustomLook()
    {
        // Set the text color for ToggleButtons to dark grey
        setColour(ToggleButton::textColourId, Colours::darkgrey);
    }

    ~CustomLook() override {}

    // Override the drawToggleButton method to customize the appearance of ToggleButtons
    void drawToggleButton(Graphics& g, ToggleButton& button, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto buttonArea = button.getLocalBounds();

        // Set the background color based on the toggle state
        if (button.getToggleState())
        {
            g.setColour(BACKGROUND_COLOR);
        }
        else
        {
            g.setColour(BACKGROUND_COLOR.darker(0.2f));
        }

        // Fill the button area with the background color
        g.fillRect(buttonArea.toFloat());
        // Set the text color based on the LookAndFeel settings
        g.setColour(button.findColour(ToggleButton::textColourId));
        // Draw the button text centered within the button area.
        g.drawFittedText(button.getButtonText(), buttonArea, Justification::centred, 1);
    }

    // Override the getTextButtonFont method to customize the font of TextButtons
    Font getTextButtonFont(TextButton&, int buttonHeight) override
    {
        return Font("Arial", buttonHeight * 0.8f, Font::plain);
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CustomLook)
};

//==============================================================================
// This class defines a custom ToggleButton subclass called Switch, which changes its text according to its toggle state
class Switch : public ToggleButton
{
private:
    ButtonsLook lookAndFeel;
    String onText, offText;
public:
    // Constructor that takes the on and off text labels
    Switch(const String& on_text, const String& off_text) : onText(on_text), offText(off_text)
    {
        setLookAndFeel(&lookAndFeel);
        setButtonText(offText);
    }

    ~Switch() override
    {
        setLookAndFeel(nullptr);
    }

    // Override the buttonStateChanged method to update the button text whenever the toggle state changes,
    // whether the change comes from a click or from the parameter that the button is attached to
    void buttonStateChanged() override
    {
        ToggleButton::buttonStateChanged();
        // Update the button text based on the toggle state
        if (getToggleState())
            setButtonText(onText);
        else
            setButtonText(offText);
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Switch)
};

//==============================================================================
// This class represents a chain of toggle buttons used for controls where multiple options need to be toggled
// It allows toggling between 2 or more options.
class ToggleChain : public Component, private Button::Listener
{
private:
    ButtonsLook lookAndFeel; // Look and feel for buttons
    int selectedId{ 0 }; // Currently selected option ID

public:
    // Called with the ID of the option that the user has clicked
    std::function<void(int)> onChange;

    ToggleChain()
    {
        setLookAndFeel(&lookAndFeel); // Set the look and feel
    }

    ~ToggleChain() override
    {
        setLookAndFeel(nullptr); // Reset the look and feel
    }

    // Adds an option with a specified name to the toggle chain
    void addOption(const String& name)
    {
        auto toggleButton = std::make_unique<ToggleButton>(name); // Create a new toggle button
        toggleButton->addListener(this); // Listen to the toggle button
        toggleButton->setClickingTogglesState(false); // Set clicking to not toggle state
        addAndMakeVisible(toggleButton.get()); // Add button to the UI
        toggleButtons.add(std::move(toggleButton)); // Add button to the array
        updateLayout(); // Update layout
    }

    void resized() override
    {
        updateLayout(); // Update layout on resize
    }

    // Sets the selection to the specified ID, without calling onChange
    void setSelection(int id)
    {
        if (id >= 0 && id < toggleButtons.size())
        {
            for (int i = 0; i < toggleButtons.size(); ++i)
                toggleButtons[i]->setToggleState(i == id, dontSendNotification); // Toggle only the specified button

            selectedId = id; // Update selected ID
        }
    }

    // Returns the ID of the currently selected option
    int getSelectedId() const
    {
        return selectedId; // Return selected ID
    }

private:
    // Handles button click events
    void buttonClicked(Button* button) override
    {
        for (int i = 0; i < toggleButtons.size(); ++i)
        {
            if (toggleButtons[i] == button && i != selectedId)
            {
                setSelection(i); // Toggle the clicked button and untoggle the others

                if (onChange)
                    onChange(i);
            }
        }
    }

    // Updates the layout of the toggle buttons
    void updateLayout()
    {
        auto bounds = getLocalBounds();
        int xPos = 0;
        for (auto& tb : toggleButtons)
        {
            tb->setBounds(bounds.removeFromLeft(50).reduced(1, 1).translated(xPos, 0)); // Set button bounds
            xPos += 5; // Adjust X position
        }
    }

    OwnedArray<ToggleButton> toggleButtons; // Array of toggle buttons

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ToggleChain)
};

//==============================================================================
// This class represents a menu for switching between the views
class SwitchButton : public Component, private Button::Listener {
private:
    // ID representing the currently selected switch option
    int switchId{ 0 };

public:
    // Called with the ID of the view that the user has clicked
    std::function<void(int)> onChange;

    // Constructor sets up the appearance, the menu buttons are added with setOptions
    SwitchButton() {
        setLookAndFeel(&lookAndFeel);
    }

    // Destructor resets the look and feel
    ~SwitchButton() override {
        setLookAndFeel(nullptr);
    }

    // Creates one menu button for each of the names, in the order of their view IDs
    void setOptions(const StringArray& names) {
        buttons.clear();

        for (auto& name : names) {
            auto* button = buttons.add(new ToggleButton());
            button->setButtonText(name.toUpperCase());

            // Disabling toggling on click for the buttons
            button->setClickingTogglesState(false);
            button->addListener(this);

            // Adding buttons to the component and making them visible
            addAndMakeVisible(button);
        }

        setSelection(switchId);

        // Updating the layout of the buttons
        updateButtonLayout();
    }

    // Sets the active view to the specified ID, without calling onChange
    void setSelection(int id) {
        if (id >= 0 && id < buttons.size()) {
            for (int i = 0; i < buttons.size(); ++i)
                buttons[i]->setToggleState(i == id, dontSendNotification);

            switchId = id;
        }
    }

    // Retrieves the currently selected option ID
    int getSwitchID() const {
        return switchId;
    }

    // Resizes and updates the layout of the menu buttons
    void resized() override {
        updateButtonLayout();
    }

private:
    // Handles button clicks by disabling other options and updating the active switch ID
    void buttonClicked(Button* button) override {
        for (int i = 0; i < buttons.size(); ++i) {
            if (buttons[i] == button && i != switchId) {
                setSelection(i);

                if (onChange)
                    onChange(i);
            }
        }
    }

    // Toggle buttons representing the menu options, in the order of their view IDs
    OwnedArray<ToggleButton> buttons;

    // Custom look and feel for the buttons
    CustomLook lookAndFeel;

    // Updates the layout of the menu buttons based on the component size
    void updateButtonLayout() {
        if (buttons.isEmpty())
            return;

        // The last button takes up whatever the division leaves over
        auto bounds = getLocalBounds();
        auto buttonWidth = getWidth() / buttons.size();

        for (int i = 0; i < buttons.size(); ++i)
            buttons[i]->setBounds(i == buttons.size() - 1 ? bounds : bounds.removeFromLeft(buttonWidth));
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SwitchButton)
};
