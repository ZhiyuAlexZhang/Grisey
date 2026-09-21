#include <JuceHeader.h>
#include "../Source/Views/Timeline.h"

//==============================================================================
struct TimelineTests : juce::UnitTest
{
    TimelineTests() : juce::UnitTest("Timeline") {}

    void runTest() override
    {
        beginTest("The clock completes thirty slots for every second, whatever the frame rate");
        for (float framesPerSecond : { 24.f, 30.f, 60.f, 120.f })
        {
            Timeline::Clock clock;
            int slots = 0;
            for (int frame = 0; frame < (int) (framesPerSecond * 10.f); ++frame)
                slots += clock.advance(1.f / framesPerSecond, true);

            expectWithinAbsoluteError(slots, 10 * Timeline::slotsPerSecond, 1);
        }

        beginTest("The clock stands still while no audio is running");
        {
            Timeline::Clock clock;
            int slots = 0;
            for (int frame = 0; frame < 300; ++frame)
                slots += clock.advance(1.f / 30.f, false);

            expectEquals(slots, 0);

            // and carries on from where it was, without making up for the pause
            expectEquals(clock.advance(1.f / 30.f + 0.001f, true), 1);
        }

        beginTest("One long frame completes several slots, but never more than the history holds");
        {
            Timeline::Clock clock;
            expectEquals(clock.advance(0.1f + 0.001f, true), 3);
            expectEquals(clock.advance(1000.f, true), Timeline::maxSlots);
        }

        beginTest("The history gives back slots by their age");
        {
            Timeline::History<int> history;
            expect(history.fromNewest(0) == nullptr);

            for (int i = 1; i <= 5; ++i)
                history.push(i);

            expectEquals(*history.fromNewest(0), 5);
            expectEquals(*history.fromNewest(4), 1);
            expect(history.fromNewest(5) == nullptr);
            expect(history.fromNewest(-1) == nullptr);
        }

        beginTest("The history keeps the newest slots once it is full");
        {
            Timeline::History<int> history;
            const int total = Timeline::maxSlots + 250;
            for (int i = 1; i <= total; ++i)
                history.push(i);

            expectEquals(*history.fromNewest(0), total);
            expectEquals(*history.fromNewest(Timeline::maxSlots - 1), total - Timeline::maxSlots + 1);
            expect(history.fromNewest(Timeline::maxSlots) == nullptr);

            history.clear();
            expect(history.fromNewest(0) == nullptr);
        }

        beginTest("A span is a whole number of slots, within what the history holds");
        {
            expectEquals(Timeline::slotsIn(15.f), 15 * Timeline::slotsPerSecond);
            expectEquals(Timeline::slotsIn(60.f), Timeline::maxSlots);
            expectEquals(Timeline::slotsIn(600.f), Timeline::maxSlots);
            expectEquals(Timeline::slotsIn(0.f), 1);
        }
    }
};

static TimelineTests timelineTests;
