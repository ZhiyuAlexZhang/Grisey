#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>

//==============================================================================
// Measures programme loudness on the audio thread, following ITU-R BS.1770-4
// and EBU R 128 (EBU Tech 3341 for the meter, and Tech 3342 for loudness range).
//
//  - Momentary:  the loudness of the last 400 ms
//  - Short-term: the loudness of the last 3 s
//  - Integrated: the loudness of everything since the last reset, gated so that
//                silence and quiet passages do not pull it down
//  - Range:      the spread between the quiet and the loud parts of the programme
//
// The signal is K-weighted, and its energy is summed over 100 ms blocks. Every
// reading is made of whole blocks, so the readings change ten times a second,
// which is the refresh rate that Tech 3341 asks for.
//
// Nothing is allocated after construction. The integrated loudness and the range
// would need an ever-growing list of blocks, so they are kept in histograms with
// 0.1 LU bins instead, which hold hours of programme in a fixed amount of memory.
class LoudnessMeter
{
public:
    static constexpr float silence = -std::numeric_limits<float>::infinity();

    struct Readings
    {
        float momentary = silence;  // LUFS
        float shortTerm = silence;  // LUFS
        float integrated = silence; // LUFS
        float range = 0.f;          // LU
        float rangeLow = silence;   // LUFS, the 10th percentile
        float rangeHigh = silence;  // LUFS, the 95th percentile
    };

    // The two biquads that make up the K-weighting: a high shelf that models the
    // head, and a high-pass. BS.1770 lists their coefficients for 48 kHz only, so
    // they are derived here from the analog prototypes, for any sample rate.
    struct Biquad
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    };

    static Biquad makeShelf(double sampleRate)
    {
        const double f0 = 1681.974450955533;
        const double gainDb = 3.999843853973347;
        const double q = 0.7071752369554196;

        const double k = std::tan(pi * f0 / sampleRate);
        const double vh = std::pow(10.0, gainDb / 20.0);
        const double vb = std::pow(vh, 0.4996667741545416);
        const double a0 = 1.0 + k / q + k * k;

        Biquad shelf;
        shelf.b0 = (vh + vb * k / q + k * k) / a0;
        shelf.b1 = 2.0 * (k * k - vh) / a0;
        shelf.b2 = (vh - vb * k / q + k * k) / a0;
        shelf.a1 = 2.0 * (k * k - 1.0) / a0;
        shelf.a2 = (1.0 - k / q + k * k) / a0;
        return shelf;
    }

    static Biquad makeHighPass(double sampleRate)
    {
        const double f0 = 38.13547087602444;
        const double q = 0.5003270373238773;

        const double k = std::tan(pi * f0 / sampleRate);
        const double a0 = 1.0 + k / q + k * k;

        Biquad highPass;
        highPass.b0 = 1.0;
        highPass.b1 = -2.0;
        highPass.b2 = 1.0;
        highPass.a1 = 2.0 * (k * k - 1.0) / a0;
        highPass.a2 = (1.0 - k / q + k * k) / a0;
        return highPass;
    }

    //==============================================================================
    // Called from prepareToPlay(), never while process() is running.
    void prepare(double newSampleRate)
    {
        const double sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;

        shelf = makeShelf(sampleRate);
        highPass = makeHighPass(sampleRate);
        samplesPerBlock = std::max(1, static_cast<int>(std::lround(sampleRate * blockSeconds)));

        clear();
    }

    // Restarts the integrated loudness and the range. Safe to call from any thread.
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

        for (int i = 0; i < numSamples; ++i)
        {
            // Left and right both have a channel weight of 1
            for (size_t channel = 0; channel < 2; ++channel)
            {
                const double weighted = filters[channel].process(static_cast<double>(input[channel][i]), shelf, highPass);
                blockEnergy += weighted * weighted;
            }

            if (++samplesInBlock == samplesPerBlock)
                finishBlock();
        }
    }

    // Returns the latest readings. Called on the GUI thread.
    Readings read() const
    {
        Readings readings;
        readings.momentary = momentary.load(std::memory_order_relaxed);
        readings.shortTerm = shortTerm.load(std::memory_order_relaxed);
        readings.integrated = integrated.load(std::memory_order_relaxed);
        readings.range = range.load(std::memory_order_relaxed);
        readings.rangeLow = rangeLow.load(std::memory_order_relaxed);
        readings.rangeHigh = rangeHigh.load(std::memory_order_relaxed);
        return readings;
    }

    // Converts between the mean square of the K-weighted signal and LUFS
    static float energyToLoudness(double energy)
    {
        return energy > 0.0 ? static_cast<float>(-0.691 + 10.0 * std::log10(energy)) : silence;
    }

    static double loudnessToEnergy(double loudness)
    {
        return std::pow(10.0, (loudness + 0.691) / 10.0);
    }

private:
    static constexpr double pi = 3.14159265358979323846;
    static constexpr double blockSeconds = 0.1;
    static constexpr int blocksInMomentary = 4;   // 400 ms
    static constexpr int blocksInShortTerm = 30;  // 3 s

    // The gates of BS.1770 and Tech 3342
    static constexpr double absoluteGate = -70.0;       // LUFS
    static constexpr double integratedRelativeGate = -10.0; // LU
    static constexpr double rangeRelativeGate = -20.0;      // LU
    static constexpr double rangeLowPercentile = 0.10;
    static constexpr double rangeHighPercentile = 0.95;

    // The histograms run from the absolute gate up to +10 LUFS in steps of 0.1 LU
    static constexpr double binsPerLu = 10.0;
    static constexpr int numBins = 800;

    //==============================================================================
    struct ChannelFilter
    {
        double process(double x, const Biquad& first, const Biquad& second)
        {
            // Transposed direct form II, which behaves well in double precision
            const double y1 = first.b0 * x + s1[0];
            s1[0] = first.b1 * x - first.a1 * y1 + s1[1];
            s1[1] = first.b2 * x - first.a2 * y1;

            const double y2 = second.b0 * y1 + s2[0];
            s2[0] = second.b1 * y1 - second.a1 * y2 + s2[1];
            s2[1] = second.b2 * y1 - second.a2 * y2;
            return y2;
        }

        std::array<double, 2> s1 { 0.0, 0.0 }, s2 { 0.0, 0.0 };
    };

    // Counts the blocks at each loudness, and keeps their exact energy, so that
    // the mean over any set of bins is exact
    struct Histogram
    {
        void clear()
        {
            counts.fill(0);
            energies.fill(0.0);
        }

        void add(double energy)
        {
            const double loudness = -0.691 + 10.0 * std::log10(energy);
            const int bin = std::clamp(static_cast<int>(std::floor((loudness - absoluteGate) * binsPerLu)), 0, numBins - 1);
            ++counts[static_cast<size_t>(bin)];
            energies[static_cast<size_t>(bin)] += energy;
        }

        // The first bin that lies entirely at or above the given loudness
        static int firstBinAbove(double loudness)
        {
            return std::clamp(static_cast<int>(std::ceil((loudness - absoluteGate) * binsPerLu - 1.0e-9)), 0, numBins);
        }

        static double binCentre(int bin)
        {
            return absoluteGate + (static_cast<double>(bin) + 0.5) / binsPerLu;
        }

        // The mean energy of the blocks from firstBin upwards, or 0 if there are none
        double meanEnergyFrom(int firstBin, std::uint64_t* countOut = nullptr) const
        {
            std::uint64_t count = 0;
            double energy = 0.0;
            for (int bin = firstBin; bin < numBins; ++bin)
            {
                count += counts[static_cast<size_t>(bin)];
                energy += energies[static_cast<size_t>(bin)];
            }

            if (countOut != nullptr)
                *countOut = count;

            return count > 0 ? energy / static_cast<double>(count) : 0.0;
        }

        std::array<std::uint64_t, numBins> counts {};
        std::array<double, numBins> energies {};
    };

    //==============================================================================
    void clear()
    {
        for (auto& filter : filters)
            filter = {};

        recentBlocks.fill(0.0);
        recentIndex = 0;
        numBlocksSeen = 0;
        blockEnergy = 0.0;
        samplesInBlock = 0;

        integratedHistogram.clear();
        rangeHistogram.clear();

        momentary.store(silence, std::memory_order_relaxed);
        shortTerm.store(silence, std::memory_order_relaxed);
        integrated.store(silence, std::memory_order_relaxed);
        range.store(0.f, std::memory_order_relaxed);
        rangeLow.store(silence, std::memory_order_relaxed);
        rangeHigh.store(silence, std::memory_order_relaxed);
    }

    // The mean energy of the most recent blocks
    double meanOfRecent(int numBlocks) const
    {
        double sum = 0.0;
        for (int i = 1; i <= numBlocks; ++i)
            sum += recentBlocks[static_cast<size_t>((recentIndex - i + blocksInShortTerm) % blocksInShortTerm)];
        return sum / static_cast<double>(numBlocks);
    }

    // Called every 100 ms
    void finishBlock()
    {
        recentBlocks[static_cast<size_t>(recentIndex)] = blockEnergy / static_cast<double>(samplesInBlock);
        recentIndex = (recentIndex + 1) % blocksInShortTerm;
        ++numBlocksSeen;
        blockEnergy = 0.0;
        samplesInBlock = 0;

        const double momentaryEnergy = meanOfRecent(blocksInMomentary);
        const double shortTermEnergy = meanOfRecent(blocksInShortTerm);
        momentary.store(energyToLoudness(momentaryEnergy), std::memory_order_relaxed);
        shortTerm.store(energyToLoudness(shortTermEnergy), std::memory_order_relaxed);

        const double gateEnergy = loudnessToEnergy(absoluteGate);

        // The integrated loudness is made of 400 ms blocks that overlap by 75%,
        // which is one every 100 ms once the first 400 ms have passed
        if (numBlocksSeen >= blocksInMomentary && momentaryEnergy >= gateEnergy)
        {
            integratedHistogram.add(momentaryEnergy);
            updateIntegrated();
        }

        // The range is made of the short-term loudness in the same way
        if (numBlocksSeen >= blocksInShortTerm && shortTermEnergy >= gateEnergy)
        {
            rangeHistogram.add(shortTermEnergy);
            updateRange();
        }
    }

    void updateIntegrated()
    {
        // The relative gate sits 10 LU below the loudness of everything above the absolute gate
        const double ungated = integratedHistogram.meanEnergyFrom(0);
        if (ungated <= 0.0)
            return;

        const double relativeGate = -0.691 + 10.0 * std::log10(ungated) + integratedRelativeGate;
        const double gated = integratedHistogram.meanEnergyFrom(Histogram::firstBinAbove(relativeGate));
        integrated.store(energyToLoudness(gated), std::memory_order_relaxed);
    }

    void updateRange()
    {
        const double ungated = rangeHistogram.meanEnergyFrom(0);
        if (ungated <= 0.0)
            return;

        const double relativeGate = -0.691 + 10.0 * std::log10(ungated) + rangeRelativeGate;
        const int firstBin = Histogram::firstBinAbove(relativeGate);

        std::uint64_t total = 0;
        rangeHistogram.meanEnergyFrom(firstBin, &total);
        if (total == 0)
            return;

        // The percentiles are the values at these positions in the sorted list of blocks
        const auto lowPosition = static_cast<std::uint64_t>(std::llround(static_cast<double>(total - 1) * rangeLowPercentile));
        const auto highPosition = static_cast<std::uint64_t>(std::llround(static_cast<double>(total - 1) * rangeHighPercentile));

        double low = 0.0, high = 0.0;
        bool foundLow = false;
        std::uint64_t seen = 0;

        for (int bin = firstBin; bin < numBins; ++bin)
        {
            seen += rangeHistogram.counts[static_cast<size_t>(bin)];

            if (!foundLow && seen > lowPosition)
            {
                low = Histogram::binCentre(bin);
                foundLow = true;
            }

            if (seen > highPosition)
            {
                high = Histogram::binCentre(bin);
                break;
            }
        }

        range.store(static_cast<float>(high - low), std::memory_order_relaxed);
        rangeLow.store(static_cast<float>(low), std::memory_order_relaxed);
        rangeHigh.store(static_cast<float>(high), std::memory_order_relaxed);
    }

    //==============================================================================
    // Audio thread state
    Biquad shelf, highPass;
    std::array<ChannelFilter, 2> filters;
    int samplesPerBlock = 4410;
    int samplesInBlock = 0;
    double blockEnergy = 0.0;

    std::array<double, blocksInShortTerm> recentBlocks {};
    int recentIndex = 0;
    std::uint64_t numBlocksSeen = 0;

    Histogram integratedHistogram, rangeHistogram;

    // Shared between the threads
    std::atomic<bool> resetRequested { false };
    std::atomic<float> momentary { silence }, shortTerm { silence }, integrated { silence };
    std::atomic<float> range { 0.f }, rangeLow { silence }, rangeHigh { silence };
};
