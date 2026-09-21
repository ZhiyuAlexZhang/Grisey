#pragma once

#include <JuceHeader.h>

//==============================================================================
// Every color, font and measurement of the interface is here, so that the look
// can be changed in one place. The palette is dark, with one accent color for
// the signal, a warm color for what is held, and red for what is over.
namespace Theme
{
    // Surfaces
    inline const juce::Colour window { 0xff12161f };      // the header, the bottom bar, and the gaps between displays
    inline const juce::Colour windowLight { 0xff1a2030 }; // the top of the header's gradient
    inline const juce::Colour display { 0xff0b0e14 };     // the wells that the displays sit in
    inline const juce::Colour track { 0xff161b26 };       // the unlit part of a meter
    inline const juce::Colour grid { 0xff1c2330 };
    inline const juce::Colour gridStrong { 0xff2a3344 };
    inline const juce::Colour menu { 0xff1a2030 };
    inline const juce::Colour menuHighlight { 0xff26304a };

    // Text
    inline const juce::Colour text { 0xffd4dbe6 };
    inline const juce::Colour textDim { 0xff7f8a9c };
    inline const juce::Colour textFaint { 0xff4a5567 };

    // Signal
    inline const juce::Colour accent { 0xff4cc2ff };   // the left or mid channel, and anything that is "the signal"
    inline const juce::Colour second { 0xffc59bff };   // the right or side channel
    inline const juce::Colour held { 0xffffc857 };     // peak holds, ticks and targets
    inline const juce::Colour good { 0xff6fdc8c };
    inline const juce::Colour over { 0xffff5c5c };     // clipping, true peaks over the limit, and out of phase

    // Measurements in pixels
    inline constexpr int headerHeight = 36;
    inline constexpr int bottomBarHeight = 30;
    inline constexpr int sideColumnWidth = 214;
    inline constexpr int gap = 1;

    // The readouts that are numbers change no more often than this, because faster cannot be read
    inline constexpr double readoutIntervalSeconds = 0.1;

    inline juce::Font font(float height, bool bold = false)
    {
        return juce::Font(juce::FontOptions().withHeight(height).withStyle(bold ? "Bold" : "Regular"));
    }

    inline juce::Font labelFont()   { return font(11.f); }
    inline juce::Font controlFont() { return font(12.5f); }

    inline int textWidth(const juce::Font& f, const juce::String& t)
    {
        return juce::GlyphArrangement::getStringWidthInt(f, t);
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
