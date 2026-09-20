#include <JuceHeader.h>
#include "../Source/Engine/LoudnessMeter.h"
#include "../Source/Engine/TruePeakDetector.h"

namespace
{
    // One stretch of a test signal: a sine at a level in dBFS, for a time in seconds
    struct Segment
    {
        double levelDb;
        double seconds;
    };

    // Feeds a 1 kHz stereo sine, in phase on both channels, through a meter. The phase
    // runs on across the segments, as it does in the EBU test files.
    template<typename Meter>
    void feedSine(Meter& meter, double sampleRate, const std::vector<Segment>& segments,
                  double frequency = 1000.0, double startPhase = 0.0, int blockSize = 512)
    {
        std::vector<float> block((size_t) blockSize);
        juce::int64 position = 0;

        for (const auto& segment : segments)
        {
            const double amplitude = std::pow(10.0, segment.levelDb / 20.0);
            auto remaining = (juce::int64) std::llround(segment.seconds * sampleRate);

            while (remaining > 0)
            {
                const int length = (int) juce::jmin<juce::int64>(blockSize, remaining);
                for (int i = 0; i < length; ++i, ++position)
                    block[(size_t) i] = (float) (amplitude * std::sin(startPhase + juce::MathConstants<double>::twoPi * frequency * (double) position / sampleRate));

                meter.process(block.data(), block.data(), length);
                remaining -= length;
            }
        }
    }

    float toDb(float gain)
    {
        return 20.f * std::log10(gain);
    }
}

//==============================================================================
struct LoudnessMeterTests : juce::UnitTest
{
    LoudnessMeterTests() : juce::UnitTest("LoudnessMeter") {}

    LoudnessMeter::Readings measure(double sampleRate, const std::vector<Segment>& segments)
    {
        LoudnessMeter meter;
        meter.prepare(sampleRate);
        feedSine(meter, sampleRate, segments);
        return meter.read();
    }

    void runTest() override
    {
        beginTest("The K-weighting at 48 kHz has the coefficients that BS.1770 lists");
        {
            const auto shelf = LoudnessMeter::makeShelf(48000.0);
            expectWithinAbsoluteError(shelf.b0, 1.53512485958697, 1.0e-9);
            expectWithinAbsoluteError(shelf.b1, -2.69169618940638, 1.0e-9);
            expectWithinAbsoluteError(shelf.b2, 1.19839281085285, 1.0e-9);
            expectWithinAbsoluteError(shelf.a1, -1.69065929318241, 1.0e-9);
            expectWithinAbsoluteError(shelf.a2, 0.73248077421585, 1.0e-9);

            const auto highPass = LoudnessMeter::makeHighPass(48000.0);
            expectWithinAbsoluteError(highPass.a1, -1.99004745483398, 1.0e-9);
            expectWithinAbsoluteError(highPass.a2, 0.99007225036621, 1.0e-9);
        }

        beginTest("Tech 3341 case 1: -23 dBFS reads -23.0 LUFS on every scale");
        {
            const auto readings = measure(48000.0, { { -23.0, 20.0 } });
            expectWithinAbsoluteError(readings.integrated, -23.f, 0.1f);
            expectWithinAbsoluteError(readings.shortTerm, -23.f, 0.1f);
            expectWithinAbsoluteError(readings.momentary, -23.f, 0.1f);
        }

        beginTest("Tech 3341 case 2: -33 dBFS reads -33.0 LUFS");
        {
            const auto readings = measure(48000.0, { { -33.0, 20.0 } });
            expectWithinAbsoluteError(readings.integrated, -33.f, 0.1f);
            expectWithinAbsoluteError(readings.shortTerm, -33.f, 0.1f);
            expectWithinAbsoluteError(readings.momentary, -33.f, 0.1f);
        }

        beginTest("Tech 3341 case 3: the relative gate ignores the quiet parts");
        expectWithinAbsoluteError(measure(48000.0, { { -36.0, 10.0 }, { -23.0, 60.0 }, { -36.0, 10.0 } }).integrated, -23.f, 0.1f);

        beginTest("Tech 3341 case 4: the absolute gate ignores the near silence");
        expectWithinAbsoluteError(measure(48000.0, { { -72.0, 10.0 }, { -36.0, 10.0 }, { -23.0, 60.0 }, { -36.0, 10.0 }, { -72.0, 10.0 } }).integrated, -23.f, 0.1f);

        beginTest("Tech 3341 case 5: levels on both sides of the relative gate");
        expectWithinAbsoluteError(measure(48000.0, { { -26.0, 20.0 }, { -20.0, 20.1 }, { -26.0, 20.0 } }).integrated, -23.f, 0.1f);

        beginTest("Tech 3341 case 9: the short-term loudness of a repeating pattern is steady");
        {
            LoudnessMeter meter;
            meter.prepare(48000.0);

            // The reading has to hold at -23.0 LUFS from 3 s onwards, so it is checked after every repeat
            for (int repeat = 0; repeat < 5; ++repeat)
            {
                feedSine(meter, 48000.0, { { -20.0, 1.34 }, { -30.0, 1.66 } });
                if (repeat >= 1)
                    expectWithinAbsoluteError(meter.read().shortTerm, -23.f, 0.1f);
            }
        }

        beginTest("Tech 3341 case 12: the momentary loudness of a repeating pattern is steady");
        {
            LoudnessMeter meter;
            meter.prepare(48000.0);

            for (int repeat = 0; repeat < 25; ++repeat)
            {
                feedSine(meter, 48000.0, { { -20.0, 0.18 }, { -30.0, 0.22 } });
                if (repeat >= 3)
                    expectWithinAbsoluteError(meter.read().momentary, -23.f, 0.1f);
            }
        }

        beginTest("The loudness does not depend on the sample rate");
        for (double sampleRate : { 44100.0, 88200.0, 96000.0, 192000.0 })
            expectWithinAbsoluteError(measure(sampleRate, { { -23.0, 10.0 } }).integrated, -23.f, 0.1f);

        beginTest("Tech 3342 case 1: levels 10 dB apart have a range of 10 LU");
        expectWithinAbsoluteError(measure(48000.0, { { -20.0, 20.0 }, { -30.0, 20.0 } }).range, 10.f, 1.f);

        beginTest("Tech 3342 case 2: levels 5 dB apart have a range of 5 LU");
        expectWithinAbsoluteError(measure(48000.0, { { -20.0, 20.0 }, { -15.0, 20.0 } }).range, 5.f, 1.f);

        beginTest("Tech 3342 case 3: levels 20 dB apart have a range of 20 LU");
        expectWithinAbsoluteError(measure(48000.0, { { -40.0, 20.0 }, { -20.0, 20.0 } }).range, 20.f, 1.f);

        beginTest("Tech 3342 case 4: the relative gate drops the quietest level");
        expectWithinAbsoluteError(measure(48000.0, { { -50.0, 20.0 }, { -35.0, 20.0 }, { -20.0, 20.0 }, { -35.0, 20.0 }, { -50.0, 20.0 } }).range, 15.f, 1.f);

        beginTest("Silence has no loudness");
        {
            LoudnessMeter meter;
            meter.prepare(48000.0);
            std::vector<float> silence(48000 * 5, 0.f);
            meter.process(silence.data(), silence.data(), (int) silence.size());

            const auto readings = meter.read();
            expect(std::isinf(readings.momentary) && readings.momentary < 0.f);
            expect(std::isinf(readings.integrated) && readings.integrated < 0.f);
            expectEquals(readings.range, 0.f);
        }

        beginTest("A reset restarts the integrated loudness");
        {
            LoudnessMeter meter;
            meter.prepare(48000.0);
            feedSine(meter, 48000.0, { { -10.0, 10.0 } });
            expectWithinAbsoluteError(meter.read().integrated, -10.f, 0.1f);

            meter.requestReset();
            feedSine(meter, 48000.0, { { -30.0, 10.0 } });
            expectWithinAbsoluteError(meter.read().integrated, -30.f, 0.1f);
        }

        beginTest("The integrated loudness holds over a long programme");
        {
            // An hour, to show that the histograms neither drift nor overflow
            const auto readings = measure(48000.0, { { -23.0, 3600.0 } });
            expectWithinAbsoluteError(readings.integrated, -23.f, 0.05f);
        }
    }
};

//==============================================================================
struct TruePeakDetectorTests : juce::UnitTest
{
    TruePeakDetectorTests() : juce::UnitTest("TruePeakDetector") {}

    // The true peak in dBTP of a sine at a fraction of the sample rate, with a starting phase in degrees
    static float measure(double sampleRate, double frequencyRatio, double phaseDegrees, double amplitude)
    {
        TruePeakDetector detector;
        detector.prepare(sampleRate);

        // A sine that starts away from a zero crossing starts with a step, and a step does
        // overshoot once it is band-limited. The steady sine is what is measured here, so the
        // first read discards the start. A whole number of seconds keeps the phase running on.
        const std::vector<Segment> second { { 20.0 * std::log10(amplitude), 1.0 } };
        feedSine(detector, sampleRate, second, sampleRate * frequencyRatio, juce::degreesToRadians(phaseDegrees));
        detector.read();
        feedSine(detector, sampleRate, second, sampleRate * frequencyRatio, juce::degreesToRadians(phaseDegrees));
        return toDb(detector.read().peak[0]);
    }

    // Tech 3341 allows a true peak meter to read up to 0.2 dB over and 0.4 dB under
    void expectTruePeak(float measured, float expected)
    {
        expect(measured <= expected + 0.2f && measured >= expected - 0.4f,
               "Measured " + juce::String(measured, 3) + " dBTP, expected " + juce::String(expected, 1));
    }

    void runTest() override
    {
        beginTest("Tech 3341 case 15: a sine at fs/4 sampled on its crests");
        expectTruePeak(measure(48000.0, 1.0 / 4.0, 0.0, 0.5), -6.f);

        beginTest("Tech 3341 case 16: a sine at fs/4 sampled 45 degrees off its crests");
        expectTruePeak(measure(48000.0, 1.0 / 4.0, 45.0, 0.5), -6.f);

        beginTest("Tech 3341 case 17: a sine at fs/6");
        expectTruePeak(measure(48000.0, 1.0 / 6.0, 60.0, 0.5), -6.f);

        beginTest("Tech 3341 case 18: a sine at fs/8");
        expectTruePeak(measure(48000.0, 1.0 / 8.0, 67.5, 0.5), -6.f);

        beginTest("Tech 3341 case 19: a peak 3 dB over full scale between the samples");
        expectTruePeak(measure(48000.0, 1.0 / 4.0, 45.0, 1.41), 3.f);

        beginTest("The sample peak alone misses what the true peak finds");
        {
            // At fs/4 and 45 degrees every sample is 3 dB below the crest
            std::vector<float> sine(48000);
            for (size_t i = 0; i < sine.size(); ++i)
                sine[i] = (float) (0.5 * std::sin(juce::MathConstants<double>::pi / 4.0 + juce::MathConstants<double>::halfPi * (double) i));

            float samplePeak = 0.f;
            for (float sample : sine)
                samplePeak = juce::jmax(samplePeak, std::abs(sample));

            expectWithinAbsoluteError(toDb(samplePeak), -9.03f, 0.01f);
        }

        beginTest("Accurate across the audio band, whatever the phase");
        for (double sampleRate : { 44100.0, 48000.0, 96000.0, 192000.0 })
        {
            for (double frequency : { 100.0, 997.0, 5000.0, 10000.0, 15000.0, 18000.0, 20000.0 })
            {
                float lowest = 100.f, highest = -100.f;
                for (int step = 0; step < 16; ++step)
                {
                    const float measured = measure(sampleRate, frequency / sampleRate, 360.0 * step / 16.0, 0.5);
                    lowest = juce::jmin(lowest, measured);
                    highest = juce::jmax(highest, measured);
                }

                logMessage(juce::String(sampleRate / 1000.0, 1) + " kHz, " + juce::String(frequency, 0) + " Hz: "
                           + juce::String(lowest + 6.02f, 3) + " to " + juce::String(highest + 6.02f, 3) + " dB error");
                expectTruePeak(lowest, -6.02f);
                expectTruePeak(highest, -6.02f);
            }
        }

        beginTest("A step overshoots between the samples, as its band-limited waveform does");
        {
            TruePeakDetector detector;
            detector.prepare(48000.0);
            std::vector<float> step(4800, 0.5f);
            detector.process(step.data(), step.data(), (int) step.size());

            const float measured = toDb(detector.read().maxPeak[0]);
            expect(measured > -6.02f + 0.5f && measured < -6.02f + 1.5f, "Measured " + juce::String(measured, 3));
        }

        beginTest("Reading restarts the peak, and a reset restarts the maximum");
        {
            TruePeakDetector detector;
            detector.prepare(48000.0);
            feedSine(detector, 48000.0, { { -6.0, 0.5 } });
            detector.read();

            auto readings = detector.read();
            expectEquals(readings.peak[0], 0.f);
            expect(readings.maxPeak[0] > 0.49f);

            detector.requestReset();
            feedSine(detector, 48000.0, { { -40.0, 0.5 } });
            expectWithinAbsoluteError(toDb(detector.read().maxPeak[0]), -40.f, 0.3f);
        }
    }
};

static LoudnessMeterTests loudnessMeterTests;
static TruePeakDetectorTests truePeakDetectorTests;
