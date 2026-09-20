#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

//==============================================================================
// A lock-free ring buffer that carries every stereo sample from the audio thread
// to the GUI thread. The audio thread writes, and the GUI thread reads the most
// recent samples whenever it draws a frame, which is what the goniometer and the
// spectrum analyzer need. Reading does not consume anything.
//
// The storage is allocated once in the constructor, so the GUI thread can keep
// reading while the host calls prepareToPlay().
class SampleRingBuffer
{
public:
    static constexpr int numChannels = 2;
    static constexpr int capacity = 1 << 16;

    SampleRingBuffer()
    {
        for (auto& channel : data)
            channel.assign(capacity, 0.f);
    }

    // Forgets everything written so far. Called from prepareToPlay().
    void reset()
    {
        totalWritten.store(0, std::memory_order_release);
    }

    // Appends a block of samples. Called on the audio thread only.
    void write(const float* left, const float* right, int numSamples)
    {
        const std::array<const float*, numChannels> source { left, right };

        // Only the newest samples fit if the block is longer than the buffer
        const int offset = std::max(0, numSamples - capacity);
        const int numToWrite = numSamples - offset;

        const auto written = totalWritten.load(std::memory_order_relaxed);
        const int start = static_cast<int>((written + static_cast<std::uint64_t>(offset)) & mask);
        const int firstPart = std::min(numToWrite, capacity - start);

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const float* samples = source[static_cast<size_t>(channel)] + offset;
            auto& destination = data[static_cast<size_t>(channel)];

            std::copy(samples, samples + firstPart, destination.begin() + start);
            std::copy(samples + firstPart, samples + numToWrite, destination.begin());
        }

        // Publishing the new count is what makes the samples visible to the reader
        totalWritten.store(written + static_cast<std::uint64_t>(numSamples), std::memory_order_release);
    }

    // Copies the most recent numSamples samples of both channels, oldest first.
    // If fewer samples have been written, the start of the output is silence.
    // Returns false if the audio thread overwrote the samples during the copy,
    // in which case the caller should keep what it drew last time.
    bool readLatest(float* left, float* right, int numSamples) const
    {
        const std::array<float*, numChannels> destination { left, right };
        numSamples = std::min(numSamples, capacity);

        const auto written = totalWritten.load(std::memory_order_acquire);
        const int available = static_cast<int>(std::min<std::uint64_t>(written, static_cast<std::uint64_t>(numSamples)));
        const int padding = numSamples - available;

        const int start = static_cast<int>((written - static_cast<std::uint64_t>(available)) & mask);
        const int firstPart = std::min(available, capacity - start);

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const auto& source = data[static_cast<size_t>(channel)];
            float* samples = destination[static_cast<size_t>(channel)];

            std::fill(samples, samples + padding, 0.f);
            std::copy(source.begin() + start, source.begin() + start + firstPart, samples + padding);
            std::copy(source.begin(), source.begin() + (available - firstPart), samples + padding + firstPart);
        }

        // The copy is intact as long as the writer has not lapped the region it came from
        const auto writtenDuringCopy = totalWritten.load(std::memory_order_acquire) - written;
        return writtenDuringCopy <= static_cast<std::uint64_t>(capacity - available);
    }

    // The number of samples written since the last reset. The GUI uses it to
    // tell whether any audio has arrived since the previous frame.
    std::uint64_t getTotalWritten() const
    {
        return totalWritten.load(std::memory_order_acquire);
    }

private:
    static constexpr std::uint64_t mask = capacity - 1;

    std::array<std::vector<float>, numChannels> data;
    std::atomic<std::uint64_t> totalWritten { 0 };
};
