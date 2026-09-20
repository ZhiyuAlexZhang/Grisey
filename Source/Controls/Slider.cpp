
#include "Slider.h"

void myLookAndFeel::drawRotarySlider(juce::Graphics& g,
    int x,
    int y,
    int width,
    int height,
    float sliderPosProportional,
    float rotaryStartAngle,
    float rotaryEndAngle,
    juce::Slider& slider)
{
    using namespace juce;

    // Create a rectangle representing the bounds of the rotary slider
    auto bounds = Rectangle<float>(x, y, width, height);

    // Check if the slider is enabled
    auto enabled = slider.isEnabled();

    // Fill the ellipse representing the rotary slider
    g.setColour(BASE_COLOR);
    g.fillEllipse(bounds);

    // Draw the outline of the ellipse representing the rotary slider
    g.setColour(enabled ? Colours::white : Colours::grey);
    g.drawEllipse(bounds, 2.f);

    // If the slider is a RotarySliderWithLabels, customize the appearance
    if (auto* rswl = dynamic_cast<RotarySliderWithLabels*>(&slider))
    {
        auto center = bounds.getCentre();

        // Create a path for the slider handle
        Path p;
        Rectangle<float> r;
        r.setLeft(center.getX() - 2);
        r.setRight(center.getX() + 2);
        r.setTop(bounds.getY() + 2);
        r.setBottom(center.getY() - rswl->getTextHeight() * 1.5);
        p.addRoundedRectangle(r, 2.f);

        // Calculate the angle for the slider handle
        auto sliderAngRad = jmap(sliderPosProportional,
            0.f, 1.f,
            rotaryStartAngle,
            rotaryEndAngle);

        // Rotate the path to match the slider angle
        p.applyTransform(AffineTransform().rotated(sliderAngRad,
            center.getX(),
            center.getY()));

        // Fill the path representing the slider handle
        g.fillPath(p);

        // Draw the label for the slider
        g.setFont(rswl->getTextHeight());
        auto text = rswl->getDisplayString();
        auto strWidth = juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), text);
        r.setSize(strWidth, rswl->getTextHeight());
        r.setCentre(bounds.getCentre());

        // Fill the background for the label
        g.setColour(enabled ? BASE_COLOR : Colours::darkgrey);
        g.fillRect(r);

        // Draw the text for the label.
        g.setColour(enabled ? Colours::white : Colours::lightgrey);
        g.drawFittedText(text, r.toNearestInt(), juce::Justification::centred, 1);
    }
}

//==============================================================================
// Implementation for the RotarySliderWithLabels class
void RotarySliderWithLabels::paint(juce::Graphics& g)
{
    using namespace juce;

    // Define the start and end angles for the rotary slider
    auto startAng = degreesToRadians(180.f + 45.f);
    auto endAng = degreesToRadians(180.f - 45.f) + MathConstants<float>::twoPi;

    // Get the range of values for the slider
    auto range = getRange();

    // Calculate the bounds for the slider
    auto sliderBounds = getSliderBounds();

    // Draw the rotary slider
    getLookAndFeel().drawRotarySlider(g,
        sliderBounds.getX(),
        sliderBounds.getY(),
        sliderBounds.getWidth(),
        sliderBounds.getHeight(),
        jmap(getValue(), range.getStart(), range.getEnd(), 0.0, 1.0),
        startAng,
        endAng,
        *this);

    // Get the center and radius of the rotary slider
    auto center = sliderBounds.toFloat().getCentre();
    auto radius = sliderBounds.getWidth() * 0.5f;

    // Set the color and font for drawing labels
    g.setColour(Colours::white);
    g.setFont(getTextHeight());

    // Draw labels around the rotary slider
    auto numChoices = labels.size();
    for (int i = 0; i < numChoices; ++i)
    {
        auto pos = labels[i].pos;
        jassert(0.f <= pos);
        jassert(pos <= 1.f);
        auto ang = jmap(labels[i].pos, 0.f, 1.f, startAng, endAng);

        auto c = center.getPointOnCircumference(radius + getTextHeight() * 0.5f + 1, ang);

        Rectangle<float> r;
        auto str = labels[i].label;

        r.setSize(juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), str), getTextHeight());

        r.setCentre(c);

        r.setY(r.getY() + getTextHeight());

        g.drawFittedText(str, r.toNearestInt(), Justification::centred, 1);
    }
}

juce::Rectangle<int> RotarySliderWithLabels::getSliderBounds() const
{
    // Get the local bounds of the component
    auto bounds = getLocalBounds();

    // Determine the size of the slider (either width or height, whichever is smaller)
    auto size = juce::jmin(bounds.getWidth(), bounds.getHeight());

    // Subtract the height of the text twice to make room for labels above and below the slider
    size -= getTextHeight() * 2;

    // Create a rectangle representing the slider with calculated size
    juce::Rectangle<int> r;
    r.setSize(size, size);

    // Set the center of the rectangle to the horizontal center of the component
    r.setCentre(bounds.getCentreX(), 0);

    // Set the Y-coordinate of the rectangle to leave space at the top for labels
    r.setY(2);

    return r;
}

juce::String RotarySliderWithLabels::getDisplayString() const
{
    // Check if the parameter is an AudioParameterChoice
    if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(param))
        return choiceParam->getCurrentChoiceName();

    // Create an empty string
    juce::String str;

    // Variable to track whether to add "k" suffix
    bool addK = false;

    // Check if the parameter is an AudioParameterFloat
    if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*>(param))
    {
        // Get the current value of the parameter
        float val = getValue();

        // If the value is greater than 999, divide it by 1000 and set addK flag to true
        if (val > 999.f)
        {
            val /= 1000.f;
            addK = true;
        }

        // Convert the float value to a string with 2 decimal places if addK flag is true
        str = juce::String(val, (addK ? 2 : 0));
    }
    else
    {
        // If the parameter is neither AudioParameterChoice nor AudioParameterFloat, assert
        jassertfalse;
    }

    // If suffix is not empty, append it to the string with a space and "k" suffix if addK is true
    if (suffix.isNotEmpty())
    {
        str << " ";
        if (addK)
            str << "k";
        str << suffix;
    }
    return str;
}
