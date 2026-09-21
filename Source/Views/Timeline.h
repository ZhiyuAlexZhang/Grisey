#pragma once

#include <JuceHeader.h>
#include "../UI/Theme.h"

//==============================================================================
// The views that show time (the spectrogram, the history and the loudness) share one timeline,
// so that the same moment is in the same place in all of them, whichever was showing when it
// happened. Time is cut into slots. One clock counts the slots, every view records something for
// every slot, whether it is showing or not, and every view shows the same span of time.
//
// A view keeps what it records as data rather than as pixels, so its picture can be made again
// when the window is resized or the span is changed, without losing anything.
namespace Timeline
{
    inline constexpr int slotsPerSecond = 30;
    inline constexpr int maxSeconds = 60;
    inline constexpr int maxSlots = slotsPerSecond * maxSeconds;

    // The number of slots in a span of time
    inline int slotsIn(float spanSeconds)
    {
        return juce::jlimit(1, maxSlots, juce::roundToInt(spanSeconds * (float)slotsPerSecond));
    }

    //==============================================================================
    // Counts time while audio is running, and says how many slots have been completed. While the
    // audio is stopped, time stands still in every view together.
    class Clock
    {
    public:
        // Returns the number of slots that have been completed since the last call
        int advance(float elapsedSeconds, bool audioRunning)
        {
            if (!audioRunning)
                return 0;

            seconds += (double)elapsedSeconds;

            const auto due = (juce::int64)(seconds * (double)slotsPerSecond);
            const int completed = (int)juce::jmin<juce::int64>(due - counted, maxSlots);
            counted = due;
            return completed;
        }

    private:
        double seconds = 0.0;
        juce::int64 counted = 0;
    };

    //==============================================================================
    // What a view has recorded in each of the last maxSlots slots
    template<typename Slot>
    class History
    {
    public:
        History() : slots((size_t)maxSlots) {}

        void push(const Slot& slot)
        {
            slots[(size_t)(written % maxSlots)] = slot;
            ++written;
        }

        void clear() { written = 0; }

        // The slot that was recorded `age` slots ago, 0 being the newest, or nullptr if there is none
        const Slot* fromNewest(int age) const
        {
            if (age < 0 || age >= maxSlots || (juce::int64)age >= written)
                return nullptr;

            return &slots[(size_t)((written - 1 - age) % maxSlots)];
        }

    private:
        std::vector<Slot> slots;
        juce::int64 written = 0;
    };

    //==============================================================================
    // Marks the time along the bottom of a plot, in the same way in every view: "now" at the
    // right edge, and the seconds before it to the left
    inline void drawTimeAxis(juce::Graphics& g, juce::Rectangle<int> plot, float spanSeconds)
    {
        const int step = spanSeconds <= 30.f ? 5 : 10;

        g.setFont(Theme::font(10.5f));
        for (int seconds = 0; seconds <= juce::roundToInt(spanSeconds); seconds += step)
        {
            const float x = (float)plot.getRight() - (float)plot.getWidth() * (float)seconds / spanSeconds;

            g.setColour(juce::Colours::white.withAlpha(0.07f));
            g.fillRect(x, (float)plot.getY(), 1.f, (float)plot.getHeight());

            g.setColour(Theme::textDim);
            const auto text = seconds == 0 ? juce::String("now")
                                           : juce::String(juce::CharPointer_UTF8("\xe2\x88\x92")) + juce::String(seconds) + " s";
            const float centre = juce::jlimit((float)plot.getX() + 16.f, (float)plot.getRight() - 12.f, x);
            g.drawText(text, juce::Rectangle<float>(50.f, 14.f).withCentre({ centre, (float)plot.getBottom() + 10.f }), juce::Justification::centred);
        }
    }
}
