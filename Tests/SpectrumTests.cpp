#include <JuceHeader.h>
#include "../Source/Engine/SpectrumEngine.h"

namespace
{
    constexpr double spectrumSampleRate = 48000.0;

    // Runs the engine over a stereo signal, a frame at a time with a hop of a quarter of the FFT,
    // as the editor does when it analyzes the newest samples in every frame
    void analyzeSignal(SpectrumEngine& engine, const std::vector<float>& left, const std::vector<float>& right)
    {
        const int size = engine.getSize();
        const int hop = size / 4;

        for (int start = 0; start + size <= (int) left.size(); start += hop)
            engine.analyze(left.data() + start, right.data() + start, (float) (hop / spectrumSampleRate));
    }

    std::vector<float> makeTone(double frequency, double amplitude, int numSamples, double phase = 0.0)
    {
        std::vector<float> tone((size_t) numSamples);
        for (int i = 0; i < numSamples; ++i)
            tone[(size_t) i] = (float) (amplitude * std::sin(phase + juce::MathConstants<double>::twoPi * frequency * i / spectrumSampleRate));
        return tone;
    }

    std::vector<float> makeNoise(int numSamples, juce::int64 seed)
    {
        juce::Random random(seed);
        std::vector<float> noise((size_t) numSamples);
        for (auto& sample : noise)
            sample = 0.25f * (random.nextFloat() * 2.f - 1.f);
        return noise;
    }

    std::vector<float> scaled(const std::vector<float>& signal, float gain)
    {
        auto result = signal;
        for (auto& sample : result)
            sample *= gain;
        return result;
    }

    std::vector<float> sum(const std::vector<float>& a, const std::vector<float>& b)
    {
        auto result = a;
        for (size_t i = 0; i < result.size(); ++i)
            result[i] += b[i];
        return result;
    }

    // The highest point of a curve, and where it is
    struct Peak
    {
        float level;
        double frequency;
    };

    Peak findPeak(const std::vector<float>& curve, const SpectrumEngine::Display& display)
    {
        const auto highest = std::max_element(curve.begin(), curve.end());
        return { *highest, SpectrumEngine::frequencyOfPoint(display, (int) std::distance(curve.begin(), highest)) };
    }

    // The point of a display that is nearest to a frequency
    size_t pointAt(const SpectrumEngine::Display& display, double frequency)
    {
        const double proportion = std::log(frequency / display.minFrequency) / std::log(display.maxFrequency / display.minFrequency);
        return (size_t) juce::jlimit(0, display.numPoints - 1, juce::roundToInt(proportion * (display.numPoints - 1)));
    }
}

//==============================================================================
struct SpectrumEngineTests : juce::UnitTest
{
    SpectrumEngineTests() : juce::UnitTest("SpectrumEngine") {}

    void runTest() override
    {
        const int length = 48000 * 2;
        SpectrumEngine::Display display;
        display.numPoints = 1024;
        std::vector<float> curve;

        beginTest("A full-scale sine reads 0 dB at its frequency");
        for (int order : { 11, 12, 13 })
        {
            SpectrumEngine engine;
            engine.setOrder(order);

            // A frequency in the middle of a bin, where the window loses nothing
            const double frequency = 100.0 * spectrumSampleRate / engine.getSize();
            const auto tone = makeTone(frequency, 1.0, length);
            analyzeSignal(engine, tone, tone);
            engine.render(SpectrumEngine::Curve::left, display, spectrumSampleRate, curve);

            const auto peak = findPeak(curve, display);
            expectWithinAbsoluteError(peak.level, 0.f, 0.1f);
            expectWithinAbsoluteError(peak.frequency, frequency, frequency * 0.01);
        }

        beginTest("A sine between two bins reads within the 1.5 dB that a Hann window loses");
        {
            SpectrumEngine engine;
            const auto tone = makeTone(100.5 * spectrumSampleRate / engine.getSize(), 1.0, length);
            analyzeSignal(engine, tone, tone);
            engine.render(SpectrumEngine::Curve::left, display, spectrumSampleRate, curve);

            const float level = findPeak(curve, display).level;
            expect(level < 0.f && level > -1.5f, "Read " + juce::String(level, 2) + " dB");
        }

        beginTest("Levels follow the signal, channel by channel");
        {
            SpectrumEngine engine;
            const double frequency = 200.0 * spectrumSampleRate / engine.getSize();
            analyzeSignal(engine, makeTone(frequency, 0.1, length), makeTone(frequency, 0.01, length));

            engine.render(SpectrumEngine::Curve::left, display, spectrumSampleRate, curve);
            expectWithinAbsoluteError(findPeak(curve, display).level, -20.f, 0.1f);

            engine.render(SpectrumEngine::Curve::right, display, spectrumSampleRate, curve);
            expectWithinAbsoluteError(findPeak(curve, display).level, -40.f, 0.1f);
        }

        beginTest("Identical channels are all mid, and inverted channels are all side");
        {
            const double frequency = 200.0 * spectrumSampleRate / 4096.0;
            const auto tone = makeTone(frequency, 0.5, length);

            SpectrumEngine inPhase;
            analyzeSignal(inPhase, tone, tone);
            inPhase.render(SpectrumEngine::Curve::mid, display, spectrumSampleRate, curve);
            expectWithinAbsoluteError(findPeak(curve, display).level, -6.02f, 0.1f);
            inPhase.render(SpectrumEngine::Curve::side, display, spectrumSampleRate, curve);
            expect(findPeak(curve, display).level < -100.f);

            SpectrumEngine outOfPhase;
            analyzeSignal(outOfPhase, tone, scaled(tone, -1.f));
            outOfPhase.render(SpectrumEngine::Curve::side, display, spectrumSampleRate, curve);
            expectWithinAbsoluteError(findPeak(curve, display).level, -6.02f, 0.1f);
            outOfPhase.render(SpectrumEngine::Curve::mid, display, spectrumSampleRate, curve);
            expect(findPeak(curve, display).level < -100.f);
        }

        beginTest("The tilt turns around 1 kHz");
        {
            SpectrumEngine engine;
            const double binWidth = spectrumSampleRate / engine.getSize();
            const double low = std::round(250.0 / binWidth) * binWidth, high = std::round(4000.0 / binWidth) * binWidth;
            const auto tones = sum(makeTone(low, 0.1, length), makeTone(high, 0.1, length));
            analyzeSignal(engine, tones, tones);

            auto tilted = display;
            tilted.tiltDbPerOctave = 4.5f;
            engine.render(SpectrumEngine::Curve::left, tilted, spectrumSampleRate, curve);

            // Two octaves either side of 1 kHz, so 9 dB down and 9 dB up
            expectWithinAbsoluteError(curve[pointAt(tilted, low)], -20.f - 9.f, 0.5f);
            expectWithinAbsoluteError(curve[pointAt(tilted, high)], -20.f + 9.f, 0.5f);
        }

        beginTest("Smoothing keeps the level of noise, and takes out its roughness");
        {
            SpectrumEngine engine;
            const auto noise = makeNoise(48000 * 4, 1);
            analyzeSignal(engine, noise, noise);

            // The spread of the curve between 1 kHz and 10 kHz, and its mean power
            auto measure = [&](float smoothingOctaves, double& meanDb, double& spreadDb)
            {
                auto smoothed = display;
                smoothed.smoothingOctaves = smoothingOctaves;
                engine.render(SpectrumEngine::Curve::left, smoothed, spectrumSampleRate, curve);

                double power = 0.0, lowest = 1000.0, highest = -1000.0;
                const size_t first = pointAt(smoothed, 1000.0), last = pointAt(smoothed, 10000.0);
                for (size_t point = first; point <= last; ++point)
                {
                    power += std::pow(10.0, curve[point] / 10.0);
                    lowest = juce::jmin(lowest, (double) curve[point]);
                    highest = juce::jmax(highest, (double) curve[point]);
                }

                meanDb = 10.0 * std::log10(power / (double) (last - first + 1));
                spreadDb = highest - lowest;
            };

            double narrowMean, narrowSpread, wideMean, wideSpread;
            measure(1.f / 12.f, narrowMean, narrowSpread);
            measure(1.f / 3.f, wideMean, wideSpread);

            expectWithinAbsoluteError(wideMean, narrowMean, 0.5);
            expect(wideSpread < narrowSpread);
            expect(wideSpread < 3.0, "Spread " + juce::String(wideSpread, 2) + " dB");
        }

        beginTest("The correlation is +1 for identical channels and -1 for inverted ones");
        {
            const auto noise = makeNoise(48000 * 2, 2);
            std::vector<float> correlation;

            SpectrumEngine inPhase;
            analyzeSignal(inPhase, noise, noise);
            inPhase.renderCorrelation(display, spectrumSampleRate, 1.f / 3.f, -90.f, correlation);
            expectWithinAbsoluteError(correlation[pointAt(display, 1000.0)], 1.f, 0.001f);

            SpectrumEngine outOfPhase;
            analyzeSignal(outOfPhase, noise, scaled(noise, -0.5f));
            outOfPhase.renderCorrelation(display, spectrumSampleRate, 1.f / 3.f, -90.f, correlation);
            expectWithinAbsoluteError(correlation[pointAt(display, 1000.0)], -1.f, 0.001f);
        }

        beginTest("The correlation of unrelated channels is near 0");
        {
            SpectrumEngine engine;
            std::vector<float> correlation;

            // The averages only reach back a fraction of a second, so the reading wanders around 0.
            // Its mean over many readings is what has to be 0.
            const auto left = makeNoise(48000 * 8, 3), right = makeNoise(48000 * 8, 4);
            const int size = engine.getSize(), hop = size / 4;
            double total = 0.0;
            int count = 0;

            for (int start = 0; start + size <= (int) left.size(); start += hop)
            {
                engine.analyze(left.data() + start, right.data() + start, (float) (hop / spectrumSampleRate));
                engine.renderCorrelation(display, spectrumSampleRate, 1.f / 3.f, -90.f, correlation);
                total += correlation[pointAt(display, 2000.0)];
                ++count;
            }

            expectWithinAbsoluteError(total / count, 0.0, 0.05);
        }

        beginTest("The correlation is measured band by band");
        {
            // In phase at 200 Hz and out of phase at 5 kHz, which a single correlation reading cannot tell apart
            const auto low = makeTone(200.0, 0.3, length), high = makeTone(5000.0, 0.3, length);

            SpectrumEngine engine;
            std::vector<float> correlation;
            analyzeSignal(engine, sum(low, high), sum(low, scaled(high, -1.f)));
            engine.renderCorrelation(display, spectrumSampleRate, 1.f / 3.f, -90.f, correlation);

            expect(correlation[pointAt(display, 200.0)] > 0.99f);
            expect(correlation[pointAt(display, 5000.0)] < -0.99f);
        }

        beginTest("Bands with no signal have no correlation");
        {
            SpectrumEngine engine;
            std::vector<float> correlation;
            const auto tone = makeTone(1000.0, 0.5, length);
            const std::vector<float> silence((size_t) length, 0.f);

            analyzeSignal(engine, silence, silence);
            engine.renderCorrelation(display, spectrumSampleRate, 1.f / 3.f, -90.f, correlation);
            expect(std::isnan(correlation[pointAt(display, 1000.0)]));
        }

        beginTest("The averages fall at about 36 dB per second");
        {
            // The power is averaged with a time constant of 0.12 s, which is 10 log10(e) / 0.12 dB per second
            SpectrumEngine engine;
            const double frequency = 200.0 * spectrumSampleRate / engine.getSize();
            const auto loud = makeTone(frequency, 1.0, 48000);
            const std::vector<float> silence((size_t) (24000 + engine.getSize()), 0.f);
            analyzeSignal(engine, loud, loud);
            analyzeSignal(engine, silence, silence);

            engine.render(SpectrumEngine::Curve::left, display, spectrumSampleRate, curve);
            expectWithinAbsoluteError(findPeak(curve, display).level, -18.f, 1.5f);
        }
    }
};

static SpectrumEngineTests spectrumEngineTests;
