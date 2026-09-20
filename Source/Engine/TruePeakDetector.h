#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

//==============================================================================
// Measures the true peak of the signal on the audio thread, in the manner of
// ITU-R BS.1770-4 Annex 2.
//
// The samples of a digital signal rarely land on the crests of the waveform that
// they describe, so the waveform that a converter reconstructs can rise above the
// largest sample, by 3 dB for a sine at a quarter of the sample rate, and by more
// for real programme. The detector oversamples the signal to find those crests.
//
// The signal is oversampled to at least 176.4 kHz: four times up to 48 kHz, twice
// up to 96 kHz, and not at all above that, as the Annex suggests. The interpolator
// is a Kaiser-windowed sinc, split into one short filter per output phase, so no
// multiplications are spent on the zeros of a zero-stuffed signal.
class TruePeakDetector
{
public:
    static constexpr int tapsPerPhase = 16;
    static constexpr int maxFactor = 4;

    struct Readings
    {
        std::array<float, 2> peak { 0.f, 0.f };    // linear gain, since the last read
        std::array<float, 2> maxPeak { 0.f, 0.f }; // linear gain, since the last reset
    };

    // The oversampling factor that is used at a given sample rate
    static int factorFor(double sampleRate)
    {
        return sampleRate < 60000.0 ? 4 : sampleRate < 120000.0 ? 2 : 1;
    }

    // Called from prepareToPlay(), never while process() is running.
    void prepare(double sampleRate)
    {
        factor = factorFor(sampleRate);
        designFilter();
        clear();
    }

    // Restarts the maximum. Safe to call from any thread.
    void requestReset()
    {
        resetRequested.store(true, std::memory_order_relaxed);
    }

    // Measures one block of audio. Called on the audio thread only.
    void process(const float* left, const float* right, int numSamples)
    {
        if (resetRequested.exchange(false, std::memory_order_relaxed))
            clear();

        const std::array<const float*, 2> input { left, right };

        for (size_t channel = 0; channel < 2; ++channel)
        {
            auto& state = history[channel];
            float blockPeak = 0.f;

            for (int i = 0; i < numSamples; ++i)
            {
                const float sample = input[channel][i];

                // The history is written twice, a filter length apart, so that the most
                // recent samples are always contiguous in memory and need no wrap-around
                const int index = positions[channel];
                state[static_cast<size_t>(index)] = sample;
                state[static_cast<size_t>(index + tapsPerPhase)] = sample;
                positions[channel] = (index + 1) % tapsPerPhase;

                // The true peak is never below the sample peak
                blockPeak = std::max(blockPeak, std::abs(sample));

                if (factor > 1)
                {
                    // The oldest of the last tapsPerPhase samples
                    const float* recent = state.data() + positions[channel];

                    for (int phase = 0; phase < factor; ++phase)
                    {
                        const float* taps = coefficients.data() + phase * tapsPerPhase;

                        float sum = 0.f;
                        for (int tap = 0; tap < tapsPerPhase; ++tap)
                            sum += taps[tap] * recent[tap];

                        blockPeak = std::max(blockPeak, std::abs(sum));
                    }
                }
            }

            storeMax(peak[channel], blockPeak);
            storeMax(maxPeak[channel], blockPeak);
        }
    }

    // Returns the latest readings and restarts the peak measurement. Called on
    // the GUI thread only.
    Readings read()
    {
        Readings readings;
        for (size_t channel = 0; channel < 2; ++channel)
        {
            readings.peak[channel] = peak[channel].exchange(0.f, std::memory_order_relaxed);
            readings.maxPeak[channel] = maxPeak[channel].load(std::memory_order_relaxed);
        }
        return readings;
    }

private:
    // Designs a sinc interpolator with a Kaiser window, and stores it as one filter per
    // output phase. The taps of each phase are stored oldest sample first.
    void designFilter()
    {
        constexpr double pi = 3.14159265358979323846;
        constexpr double beta = 8.0;

        const int length = factor * tapsPerPhase;
        const double centre = 0.5 * static_cast<double>(length - 1);
        coefficients.assign(static_cast<size_t>(maxFactor * tapsPerPhase), 0.f);

        for (int phase = 0; phase < factor; ++phase)
        {
            std::array<double, tapsPerPhase> taps {};
            double sum = 0.0;

            for (int tap = 0; tap < tapsPerPhase; ++tap)
            {
                const int n = phase + factor * tap;
                const double t = (static_cast<double>(n) - centre) / static_cast<double>(factor);
                const double sinc = std::abs(t) < 1.0e-12 ? 1.0 : std::sin(pi * t) / (pi * t);

                const double position = (static_cast<double>(n) - centre) / centre;
                const double window = besselI0(beta * std::sqrt(std::max(0.0, 1.0 - position * position))) / besselI0(beta);

                taps[static_cast<size_t>(tap)] = sinc * window;
                sum += sinc * window;
            }

            // Each phase passes a constant signal unchanged
            // Tap 0 weights the newest sample, so the order is reversed to match the history
            for (int tap = 0; tap < tapsPerPhase; ++tap)
                coefficients[static_cast<size_t>(phase * tapsPerPhase + (tapsPerPhase - 1 - tap))] = static_cast<float>(taps[static_cast<size_t>(tap)] / sum);
        }
    }

    // The zeroth-order modified Bessel function of the first kind, from its power series
    static double besselI0(double x)
    {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 50; ++k)
        {
            term *= (x / (2.0 * k)) * (x / (2.0 * k));
            sum += term;
            if (term < 1.0e-16 * sum)
                break;
        }
        return sum;
    }

    void clear()
    {
        for (auto& state : history)
            state.fill(0.f);

        positions = { 0, 0 };

        for (auto& value : peak) value.store(0.f, std::memory_order_relaxed);
        for (auto& value : maxPeak) value.store(0.f, std::memory_order_relaxed);
    }

    static void storeMax(std::atomic<float>& target, float value)
    {
        float current = target.load(std::memory_order_relaxed);
        while (value > current && ! target.compare_exchange_weak(current, value, std::memory_order_relaxed))
        {
        }
    }

    // Audio thread state
    int factor = 4;
    std::vector<float> coefficients;
    std::array<std::array<float, 2 * tapsPerPhase>, 2> history {};
    std::array<int, 2> positions { 0, 0 };

    // Shared between the threads
    std::atomic<bool> resetRequested { false };
    std::array<std::atomic<float>, 2> peak { 0.f, 0.f };
    std::array<std::atomic<float>, 2> maxPeak { 0.f, 0.f };
};
