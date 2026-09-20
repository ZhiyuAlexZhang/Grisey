#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>
#include "AtomicMax.h"

//==============================================================================
// Measures the signal on the audio thread, from every sample, and publishes the
// results for the GUI thread to read once per frame.
//
//  - Peak:        the largest absolute sample since the GUI last read it, so a
//                 peak is never missed however many blocks arrive per frame
//  - RMS:         the root mean square over a sliding 300 ms window
//  - Correlation: the normalized cross-correlation of left and right, with a
//                 fast (50 ms) and a slow (user-set) integration time
class MeterEngine
{
public:
    static constexpr double rmsWindowSeconds = 0.3;
    static constexpr double fastCorrelationSeconds = 0.05;

    struct Readings
    {
        std::array<float, 2> peak { 0.f, 0.f }; // linear gain
        std::array<float, 2> rms { 0.f, 0.f };  // linear gain
        float correlationFast = 0.f;            // -1 to +1
        float correlationSlow = 0.f;            // -1 to +1
    };

    // Allocates the RMS window. Called from prepareToPlay(), never while
    // process() is running.
    void prepare(double newSampleRate)
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;

        const auto windowLength = static_cast<size_t>(std::max(1.0, std::round(rmsWindowSeconds * sampleRate)));
        for (auto& window : squares)
            window.assign(windowLength, 0.f);

        windowIndex = 0;
        sumOfSquares = { 0.0, 0.0 };

        fast = {};
        slow = {};
        fastCoefficient = coefficientFor(fastCorrelationSeconds);

        for (auto& value : peak) value.store(0.f, std::memory_order_relaxed);
        for (auto& value : rms) value.store(0.f, std::memory_order_relaxed);
        correlationFast.store(0.f, std::memory_order_relaxed);
        correlationSlow.store(0.f, std::memory_order_relaxed);
    }

    // Sets the integration time of the slow correlation reading. Safe to call
    // from any thread.
    void setSlowCorrelationSeconds(float seconds)
    {
        slowSeconds.store(seconds, std::memory_order_relaxed);
    }

    // Measures one block. Called on the audio thread only.
    void process(const float* left, const float* right, int numSamples)
    {
        if (squares[0].empty() || numSamples <= 0)
            return;

        // The integration time can change at any moment, and one exp() per block costs nothing
        const double slowCoefficient = coefficientFor(static_cast<double>(slowSeconds.load(std::memory_order_relaxed)));

        const std::array<const float*, 2> input { left, right };
        const size_t windowLength = squares[0].size();
        std::array<float, 2> blockPeak { 0.f, 0.f };

        for (int i = 0; i < numSamples; ++i)
        {
            const float l = input[0][i];
            const float r = input[1][i];

            blockPeak[0] = std::max(blockPeak[0], std::abs(l));
            blockPeak[1] = std::max(blockPeak[1], std::abs(r));

            // Slide the RMS window along by one sample
            const std::array<float, 2> squared { l * l, r * r };
            for (size_t channel = 0; channel < 2; ++channel)
            {
                sumOfSquares[channel] += static_cast<double>(squared[channel]) - static_cast<double>(squares[channel][windowIndex]);
                squares[channel][windowIndex] = squared[channel];
            }

            if (++windowIndex == windowLength)
            {
                windowIndex = 0;

                // Adding and subtracting leaves a rounding error in the running
                // sum, so it is recomputed exactly once per window
                for (size_t channel = 0; channel < 2; ++channel)
                {
                    double exact = 0.0;
                    for (float value : squares[channel])
                        exact += static_cast<double>(value);
                    sumOfSquares[channel] = exact;
                }
            }

            fast.update(l, r, fastCoefficient);
            slow.update(l, r, slowCoefficient);
        }

        for (size_t channel = 0; channel < 2; ++channel)
        {
            storeMax(peak[channel], blockPeak[channel]);

            const double meanSquare = std::max(0.0, sumOfSquares[channel]) / static_cast<double>(windowLength);
            rms[channel].store(static_cast<float>(std::sqrt(meanSquare)), std::memory_order_relaxed);
        }

        correlationFast.store(fast.getCorrelation(), std::memory_order_relaxed);
        correlationSlow.store(slow.getCorrelation(), std::memory_order_relaxed);
    }

    // Returns the latest readings and restarts the peak measurement. Called on
    // the GUI thread only.
    Readings read()
    {
        Readings readings;

        for (size_t channel = 0; channel < 2; ++channel)
        {
            readings.peak[channel] = peak[channel].exchange(0.f, std::memory_order_relaxed);
            readings.rms[channel] = rms[channel].load(std::memory_order_relaxed);
        }

        readings.correlationFast = correlationFast.load(std::memory_order_relaxed);
        readings.correlationSlow = correlationSlow.load(std::memory_order_relaxed);
        return readings;
    }

private:
    // One-pole averages of L*R, L*L and R*R, from which the correlation follows
    struct CorrelationIntegrator
    {
        void update(float l, float r, double coefficient)
        {
            const double dl = static_cast<double>(l);
            const double dr = static_cast<double>(r);
            lr = dl * dr + coefficient * (lr - dl * dr);
            ll = dl * dl + coefficient * (ll - dl * dl);
            rr = dr * dr + coefficient * (rr - dr * dr);
        }

        float getCorrelation() const
        {
            // Below roughly -90 dB there is no meaningful phase relationship
            const double energy = ll * rr;
            if (energy < 1.0e-18)
                return 0.f;

            return static_cast<float>(std::clamp(lr / std::sqrt(energy), -1.0, 1.0));
        }

        double lr = 0.0, ll = 0.0, rr = 0.0;
    };

    double coefficientFor(double seconds) const
    {
        return std::exp(-1.0 / (std::max(0.001, seconds) * sampleRate));
    }

    double sampleRate = 44100.0;

    // Audio thread state
    std::array<std::vector<float>, 2> squares;
    std::array<double, 2> sumOfSquares { 0.0, 0.0 };
    size_t windowIndex = 0;
    CorrelationIntegrator fast, slow;
    double fastCoefficient = 0.0;

    // Shared between the threads
    std::atomic<float> slowSeconds { 0.1f };
    std::array<std::atomic<float>, 2> peak { 0.f, 0.f };
    std::array<std::atomic<float>, 2> rms { 0.f, 0.f };
    std::atomic<float> correlationFast { 0.f }, correlationSlow { 0.f };
};
