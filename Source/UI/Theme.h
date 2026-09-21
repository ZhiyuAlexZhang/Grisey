#pragma once

#include <JuceHeader.h>

//==============================================================================
// Every color, font and measurement of the interface is here, so that the look
// can be changed in one place.
//
// The look follows the design language of FabFilter's plugins: charcoal chrome with
// sculpted edges around a display that is almost black, a yellow curve for the main
// signal and a blue one for the second, thin grey grids, and light humanist type
// with grey labels in front of pale values.
namespace Theme
{
    // Chrome: the header and the bottom bar
    inline const juce::Colour chromeTop { 0xff201d22 };      // the raised parts run from this at the top
    inline const juce::Colour chromeBottom { 0xff353238 };   // to this at the bottom
    inline const juce::Colour chromeInsetTop { 0xff1c1a20 }; // the recessed strip that holds the tabs
    inline const juce::Colour chromeInsetBottom { 0xff070709 };
    inline const juce::Colour footerTop { 0xff1c1b1e };
    inline const juce::Colour footerBottom { 0xff19161e };
    inline const juce::Colour edge { 0xff2a292d };           // the hairlines around the chrome and between displays
    inline const juce::Colour window { 0xff000000 };         // what shows in the gaps between the displays

    // Displays
    inline const juce::Colour displayTop { 0xff000000 };     // a display runs from black at the top
    inline const juce::Colour displayBottom { 0xff0b0614 };  // to a deep purple at the bottom
    inline const juce::Colour display { 0xff05030a };        // one color for where a gradient cannot be used
    inline const juce::Colour track { 0xff17151c };          // the unlit part of a meter
    inline const juce::Colour grid { 0xff201f22 };
    inline const juce::Colour gridStrong { 0xff2f2e30 };

    // Floating panels and menus
    inline const juce::Colour panel { 0xff1d1b22 };
    inline const juce::Colour panelEdge { 0xff45424b };
    inline const juce::Colour menu { 0xff1d1b22 };
    inline const juce::Colour menuHighlight { 0xff1c4e8c };  // the solid blue bar under the item of a menu that the mouse is on

    // Text: grey labels in front of pale values
    inline const juce::Colour text { 0xffd1d1d2 };
    inline const juce::Colour textDim { 0xff868587 };
    inline const juce::Colour textFaint { 0xff58565c };
    inline const juce::Colour knobLabel { 0xffb9bdc0 };
    inline const juce::Colour wordmark { 0xfff0e0e0 };  // the name of the plugin in the header

    // Signal
    inline const juce::Colour accent { 0xffffc435 };   // yellow: the left or mid channel, and anything that is "the signal"
    inline const juce::Colour second { 0xff3f9fdc };   // blue: the right or side channel
    inline const juce::Colour secondDeep { 0xff154767 }; // the deep blue of a knob's ring
    inline const juce::Colour held { 0xffffffff };     // peak holds and ticks
    inline const juce::Colour target { 0xff59fb18 };   // what to aim for
    inline const juce::Colour good { 0xff59fb18 };
    inline const juce::Colour over { 0xffdb5031 };     // clipping, true peaks over the limit, and out of phase

    // How strongly a curve is filled beneath its line. The second curve usually lies over the first,
    // as left and right do, so its fill is faint, or the two would mix into a muddy color.
    inline constexpr float accentFillAlpha = 0.20f;
    inline constexpr float secondFillAlpha = 0.07f;
    inline constexpr float curveThickness = 2.f;

    // Measurements in pixels
    inline constexpr int headerHeight = 38;
    inline constexpr int bottomBarHeight = 26;
    inline constexpr int sideColumnWidth = 214;
    inline constexpr int gap = 1;

    // The readouts that are numbers change no more often than this, because faster cannot be read
    inline constexpr double readoutIntervalSeconds = 0.1;

    // A light humanist sans-serif, from what each system has
    inline juce::String typefaceName()
    {
       #if JUCE_MAC
        return "Avenir Next";
       #elif JUCE_WINDOWS
        return "Segoe UI";
       #else
        return juce::Font::getDefaultSansSerifFontName();
       #endif
    }

    // What each of those typefaces calls its heavier weight
    inline juce::String boldStyleName()
    {
       #if JUCE_MAC
        return "Demi Bold";
       #elif JUCE_WINDOWS
        return "Semibold";
       #else
        return "Bold";
       #endif
    }

    inline juce::Font font(float height, bool bold = false)
    {
        return juce::Font(juce::FontOptions(typefaceName(), height, juce::Font::plain).withStyle(bold ? boldStyleName() : juce::String("Regular")));
    }

    // Small capitals with a little space between the letters, for the names of things
    inline juce::Font labelFont()   { return font(10.5f, true).withExtraKerningFactor(0.06f); }
    inline juce::Font controlFont() { return font(12.5f); }

    inline int textWidth(const juce::Font& f, const juce::String& t)
    {
        return juce::GlyphArrangement::getStringWidthInt(f, t);
    }

    // Fills a display with its background, which is given the height of the whole display
    // so that views that draw it in parts stay in step
    inline void fillDisplay(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        g.setGradientFill(juce::ColourGradient(displayTop, 0.f, (float)bounds.getY() + 0.25f * (float)bounds.getHeight(),
                                               displayBottom, 0.f, (float)bounds.getBottom(), false));
        g.fillRect(bounds);
    }

    // Draws a floating panel: dark and rounded, with a lighter edge and a soft shadow under it
    inline void drawPanel(juce::Graphics& g, juce::Rectangle<float> bounds, float alpha = 0.94f)
    {
        juce::Path outline;
        outline.addRoundedRectangle(bounds, 6.f);

        juce::DropShadow(juce::Colours::black.withAlpha(0.55f), 12, { 0, 4 }).drawForPath(g, outline);
        g.setColour(panel.withAlpha(alpha));
        g.fillPath(outline);
        g.setColour(panelEdge);
        g.strokePath(outline, juce::PathStrokeType(1.f));
    }

    // Formats a level in decibels with one decimal place, a true minus sign, and a dash for silence
    inline juce::String formatDb(float decibels, float silenceBelow = -100.f)
    {
        if (!(decibels > silenceBelow))
            return juce::String(juce::CharPointer_UTF8("\xe2\x80\x93"));

        const auto number = juce::String(std::abs(decibels), 1);
        return decibels < -0.05f ? juce::String(juce::CharPointer_UTF8("\xe2\x88\x92")) + number
             : decibels > 0.05f ? "+" + number
             : juce::String("0.0");
    }

    // Formats a frequency as it is spoken: 440 Hz, 1.25 kHz
    inline juce::String formatFrequency(double hertz)
    {
        return hertz < 1000.0 ? juce::String(juce::roundToInt(hertz)) + " Hz"
                              : juce::String(hertz / 1000.0, hertz < 10000.0 ? 2 : 1) + " kHz";
    }

    // Names the note nearest to a frequency, with how far off it is in cents: A4, C#3 +12c
    inline juce::String formatNote(double hertz)
    {
        if (hertz <= 0.0)
            return {};

        static const char* const names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

        const double midi = 69.0 + 12.0 * std::log2(hertz / 440.0);
        const int nearest = juce::roundToInt(midi);
        const int cents = juce::roundToInt((midi - nearest) * 100.0);

        juce::String note = juce::String(names[((nearest % 12) + 12) % 12]) + juce::String(nearest / 12 - 1);
        if (cents != 0)
            note << (cents > 0 ? " +" : " ") << cents << "c";

        return note;
    }
}
