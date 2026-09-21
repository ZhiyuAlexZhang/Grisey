#pragma once

#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

//==============================================================================
// Analyzes the spectrum of the most recent audio. It runs on the GUI thread, once
// per frame, on samples that it is given from the SampleRingBuffer. Because every
// frame analyzes the newest samples, successive analyses overlap.
//
// Each analysis takes one complex FFT of the left channel and one of the right,
// and averages three spectra over time: the power of the left, the power of the
// right, and the real part of their cross-spectrum. Everything that is displayed
// follows from those three:
//
//  - Left and right:  the two power spectra
//  - Mid and side:    (L + R + 2 cross) / 4 and (L + R - 2 cross) / 4
//  - Correlation:     cross / sqrt(L * R) over a band, which is the correlation of
//                     the two channels within that band
//
// The power is scaled so that a full-scale sine reads 0 dB.
class SpectrumEngine
{
public:
    enum class Curve
    {
        left,
        right,
        mid,
        side
    };

    // How a spectrum is laid out for display
    struct Display
    {
        int numPoints = 512;            // points along the frequency axis, spaced logarithmically
        double minFrequency = 20.0;
        double maxFrequency = 20000.0;
        float tiltDbPerOctave = 0.f;    // slope added around 1 kHz, 4.5 makes pink-ish music look flat
        float smoothingOctaves = 0.f;   // width of the averaging band, 0 for none
    };

    static constexpr float silenceDb = -150.f;
    static constexpr double averagingSeconds = 0.12;

    SpectrumEngine()
    {
        setOrder(12);
    }

    // Sets the FFT size to 2^order samples, and forgets the averages
    void setOrder(int newOrder)
    {
        order = juce::jlimit(8, 15, newOrder);
        size = 1 << order;
        numBins = size / 2 + 1;

        fft = std::make_unique<juce::dsp::FFT>(order);

        // A periodic Hann window, whose sum is exactly half its length
        window.resize(static_cast<size_t>(size));
        for (int n = 0; n < size; ++n)
            window[static_cast<size_t>(n)] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * static_cast<float>(n) / static_cast<float>(size));

        for (auto& buffer : fftData)
            buffer.assign(static_cast<size_t>(size) * 2, 0.f);

        for (auto* spectrum : { &powerLeft, &powerRight, &cross })
            spectrum->assign(static_cast<size_t>(numBins), 0.0);

        cumulative.assign(static_cast<size_t>(numBins) + 1, 0.0);
        hasAverage = false;
    }

    int getOrder() const { return order; }
    int getSize() const { return size; }

    // Forgets the averages, so that the next analysis starts afresh
    void reset()
    {
        for (auto* spectrum : { &powerLeft, &powerRight, &cross })
            std::fill(spectrum->begin(), spectrum->end(), 0.0);

        hasAverage = false;
    }

    // Analyzes getSize() samples of each channel, and folds the result into the averages.
    // elapsedSeconds is the time since the previous analysis, which sets how far the
    // averages move towards the new spectrum.
    void analyze(const float* left, const float* right, float elapsedSeconds)
    {
        const std::array<const float*, 2> input { left, right };

        for (size_t channel = 0; channel < 2; ++channel)
        {
            auto& data = fftData[channel];
            for (int n = 0; n < size; ++n)
                data[static_cast<size_t>(n)] = input[channel][n] * window[static_cast<size_t>(n)];

            std::fill(data.begin() + size, data.end(), 0.f);
            fft->performRealOnlyForwardTransform(data.data(), true);
        }

        // A sine of amplitude 1 has a magnitude of sum(window) / 2 = size / 4 in its bin
        const double scale = 16.0 / (static_cast<double>(size) * static_cast<double>(size));
        const double keep = hasAverage ? std::exp(-static_cast<double>(elapsedSeconds) / averagingSeconds) : 0.0;

        for (int bin = 0; bin < numBins; ++bin)
        {
            const auto index = static_cast<size_t>(bin);
            const double lRe = fftData[0][2 * index], lIm = fftData[0][2 * index + 1];
            const double rRe = fftData[1][2 * index], rIm = fftData[1][2 * index + 1];

            const double newLeft = (lRe * lRe + lIm * lIm) * scale;
            const double newRight = (rRe * rRe + rIm * rIm) * scale;
            const double newCross = (lRe * rRe + lIm * rIm) * scale;

            powerLeft[index] = newLeft + keep * (powerLeft[index] - newLeft);
            powerRight[index] = newRight + keep * (powerRight[index] - newRight);
            cross[index] = newCross + keep * (cross[index] - newCross);
        }

        hasAverage = true;
    }

    // Lays out one curve for display, in decibels
    void render(Curve curve, const Display& display, double sampleRate, std::vector<float>& decibels)
    {
        decibels.resize(static_cast<size_t>(display.numPoints));

        // Only the band averages of smoothing need the running total
        if (display.smoothingOctaves > 0.f)
            accumulate([this, curve](size_t bin) { return powerOf(curve, bin); });

        forEachPoint(display, sampleRate, [&](int point, double frequency, double lowBin, double highBin, double cellLowBin, double cellHighBin)
        {
            double power = 0.0;

            if (display.smoothingOctaves > 0.f && highBin - lowBin >= 1.0)
            {
                // Smoothing averages the power across the band
                power = bandSum(lowBin, highBin) / (highBin - lowBin);
            }
            else
            {
                // Without smoothing, a point shows the strongest of the bins that lie in its cell, so that
                // a tone keeps its level however narrow it is on the screen. The cells tile the frequency
                // axis, so every bin is shown at full value by exactly one point.
                const auto first = static_cast<int>(std::ceil(cellLowBin));
                const auto last = juce::jmin(static_cast<int>(std::floor(cellHighBin)), numBins - 1);

                for (int bin = juce::jmax(0, first); bin <= last; ++bin)
                    power = juce::jmax(power, powerOf(curve, static_cast<size_t>(bin)));

                if (first > last)
                {
                    // Where the points are closer together than the bins, as they are at low frequencies,
                    // the curve runs through the bins as a smooth spline of their levels in decibels.
                    // Straight lines between them would show as a row of arches. The spline keeps the
                    // shape of the bins: it never goes above the higher or below the lower of the two
                    // that it runs between, which matters because a tone stands 100 dB above the bins
                    // beside it, and a spline that was free to overshoot would read several decibels high.
                    const double position = juce::jlimit(0.0, static_cast<double>(numBins - 1), 0.5 * (cellLowBin + cellHighBin));
                    const int lower = static_cast<int>(position);
                    const double t = position - static_cast<double>(lower);

                    auto levelOf = [&](int bin)
                    {
                        const double p = powerOf(curve, static_cast<size_t>(juce::jlimit(0, numBins - 1, bin)));
                        return 10.0 * std::log10(juce::jmax(p, 1.0e-15));
                    };

                    const double p0 = levelOf(lower - 1), p1 = levelOf(lower), p2 = levelOf(lower + 1), p3 = levelOf(lower + 2);

                    // The slope at a bin is the harmonic mean of the slopes on either side of it,
                    // and zero at a peak or a dip (Fritsch and Carlson's monotone cubic)
                    auto slopeBetween = [](double before, double after)
                    {
                        return before * after > 0.0 ? 2.0 * before * after / (before + after) : 0.0;
                    };

                    const double m1 = slopeBetween(p1 - p0, p2 - p1);
                    const double m2 = slopeBetween(p2 - p1, p3 - p2);

                    // A cubic Hermite curve from p1 to p2 with those slopes
                    const double t2 = t * t, t3 = t2 * t;
                    const double level = (2.0 * t3 - 3.0 * t2 + 1.0) * p1 + (t3 - 2.0 * t2 + t) * m1
                                       + (-2.0 * t3 + 3.0 * t2) * p2 + (t3 - t2) * m2;
                    power = std::pow(10.0, level / 10.0);
                }
            }

            const double tilt = static_cast<double>(display.tiltDbPerOctave) * std::log2(frequency / 1000.0);
            const double level = power > 0.0 ? 10.0 * std::log10(power) + tilt : static_cast<double>(silenceDb);
            decibels[static_cast<size_t>(point)] = static_cast<float>(juce::jmax(level, static_cast<double>(silenceDb)));
        });
    }

    // Lays out the correlation of left and right for display, from -1 to +1 per point.
    // Each point covers a band of at least bandOctaves, because the correlation of a
    // single bin says nothing. Points whose band is quieter than quietDb are NaN.
    void renderCorrelation(const Display& display, double sampleRate, float bandOctaves, float quietDb, std::vector<float>& correlation)
    {
        correlation.resize(static_cast<size_t>(display.numPoints));

        auto band = display;
        band.smoothingOctaves = juce::jmax(bandOctaves, display.smoothingOctaves);

        // Three passes, one for each of the averaged spectra
        std::vector<double>& sums = scratch;
        sums.assign(static_cast<size_t>(display.numPoints) * 3, 0.0);

        const std::array<const std::vector<double>*, 3> spectra { &powerLeft, &powerRight, &cross };
        for (size_t s = 0; s < spectra.size(); ++s)
        {
            accumulate([&](size_t bin) { return (*spectra[s])[bin]; });

            forEachPoint(band, sampleRate, [&](int point, double, double lowBin, double highBin, double, double)
            {
                // A band narrower than a bin still takes in the whole of that bin
                const double centre = 0.5 * (lowBin + highBin);
                const double halfWidth = juce::jmax(0.5, 0.5 * (highBin - lowBin));
                sums[static_cast<size_t>(point) * 3 + s] = bandSum(centre - halfWidth, centre + halfWidth) / (2.0 * halfWidth);
            });
        }

        const double quietPower = std::pow(10.0, static_cast<double>(quietDb) / 10.0);

        for (int point = 0; point < display.numPoints; ++point)
        {
            const double left = sums[static_cast<size_t>(point) * 3];
            const double right = sums[static_cast<size_t>(point) * 3 + 1];
            const double crossSum = sums[static_cast<size_t>(point) * 3 + 2];

            correlation[static_cast<size_t>(point)] = (left + right < quietPower || left <= 0.0 || right <= 0.0)
                ? std::numeric_limits<float>::quiet_NaN()
                : static_cast<float>(juce::jlimit(-1.0, 1.0, crossSum / std::sqrt(left * right)));
        }
    }

    // The frequency of a display point
    static double frequencyOfPoint(const Display& display, int point)
    {
        const double proportion = display.numPoints > 1 ? static_cast<double>(point) / static_cast<double>(display.numPoints - 1) : 0.0;
        return display.minFrequency * std::pow(display.maxFrequency / display.minFrequency, proportion);
    }

private:
    double powerOf(Curve curve, size_t bin) const
    {
        switch (curve)
        {
            case Curve::left:  return powerLeft[bin];
            case Curve::right: return powerRight[bin];
            case Curve::mid:   return juce::jmax(0.0, 0.25 * (powerLeft[bin] + powerRight[bin] + 2.0 * cross[bin]));
            case Curve::side:  return juce::jmax(0.0, 0.25 * (powerLeft[bin] + powerRight[bin] - 2.0 * cross[bin]));
        }

        return 0.0;
    }

    // Builds the running total of a spectrum, in which bin k covers the span from k - 0.5 to k + 0.5,
    // so that the sum over any band is the difference of two look-ups
    template<typename PowerOfBin>
    void accumulate(PowerOfBin&& powerOfBin)
    {
        cumulative[0] = 0.0;
        for (int bin = 0; bin < numBins; ++bin)
            cumulative[static_cast<size_t>(bin) + 1] = cumulative[static_cast<size_t>(bin)] + powerOfBin(static_cast<size_t>(bin));
    }

    // The running total at a fractional bin position
    double cumulativeAt(double binPosition) const
    {
        const double shifted = juce::jlimit(0.0, static_cast<double>(numBins), binPosition + 0.5);
        const auto whole = static_cast<size_t>(shifted);
        if (whole >= static_cast<size_t>(numBins))
            return cumulative[static_cast<size_t>(numBins)];

        const double fraction = shifted - static_cast<double>(whole);
        return cumulative[whole] + fraction * (cumulative[whole + 1] - cumulative[whole]);
    }

    double bandSum(double lowBin, double highBin) const
    {
        return cumulativeAt(highBin) - cumulativeAt(lowBin);
    }

    // Calls the function for every display point with its frequency, the band of bins that it covers,
    // and its cell. The band is as wide as the smoothing asks, and never narrower than the cell, which
    // is the point's share of the frequency axis.
    template<typename Function>
    void forEachPoint(const Display& display, double sampleRate, Function&& function) const
    {
        const double octaves = std::log2(display.maxFrequency / display.minFrequency);
        const double spacing = display.numPoints > 1 ? octaves / static_cast<double>(display.numPoints - 1) : octaves;
        const double halfWidth = 0.5 * juce::jmax(spacing, static_cast<double>(display.smoothingOctaves));
        const double binsPerHz = static_cast<double>(size) / sampleRate;
        const double lowBinsPerHz = std::pow(2.0, -halfWidth) * binsPerHz;
        const double highBinsPerHz = std::pow(2.0, halfWidth) * binsPerHz;
        const double cellLowBinsPerHz = std::pow(2.0, -0.5 * spacing) * binsPerHz;
        const double cellHighBinsPerHz = std::pow(2.0, 0.5 * spacing) * binsPerHz;

        for (int point = 0; point < display.numPoints; ++point)
        {
            const double frequency = frequencyOfPoint(display, point);
            function(point, frequency, frequency * lowBinsPerHz, frequency * highBinsPerHz,
                     frequency * cellLowBinsPerHz, frequency * cellHighBinsPerHz);
        }
    }

    int order = 12, size = 4096, numBins = 2049;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window;
    std::array<std::vector<float>, 2> fftData;

    // The spectra averaged over time
    std::vector<double> powerLeft, powerRight, cross;
    bool hasAverage = false;

    // Working storage, kept between calls so that a frame allocates nothing
    std::vector<double> cumulative, scratch;
};
