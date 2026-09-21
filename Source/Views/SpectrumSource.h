#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../Engine/SpectrumEngine.h"

//==============================================================================
// Runs the SpectrumEngine on the newest audio, once per frame. The analyzer and
// the spectrogram both draw from it, so the FFTs are taken once for both.
class SpectrumSource
{
public:
    explicit SpectrumSource(MultiMeterAudioProcessor& p) : audioProcessor(p) {}

    // Analyzes the newest audio with an FFT of 2^order samples. Returns false, and leaves the
    // spectra as they were, if no audio has arrived since the last call.
    bool update(float elapsedSeconds, int order)
    {
        if (order != engine.getOrder())
        {
            engine.setOrder(order);
            lastTotalWritten = 0;
        }

        auto& ringBuffer = audioProcessor.sampleRingBuffer;

        const auto totalWritten = ringBuffer.getTotalWritten();
        if (totalWritten == lastTotalWritten)
            return false;

        analysisBuffer.setSize(2, engine.getSize(), false, false, true);

        // Keep the previous spectra if the audio thread overwrote the samples during the copy
        if (!ringBuffer.readLatest(analysisBuffer.getWritePointer(0), analysisBuffer.getWritePointer(1), engine.getSize()))
            return false;

        lastTotalWritten = totalWritten;
        engine.analyze(analysisBuffer.getReadPointer(0), analysisBuffer.getReadPointer(1), elapsedSeconds);
        return true;
    }

    SpectrumEngine& getEngine() { return engine; }

    double getSampleRate() const
    {
        const double sampleRate = audioProcessor.getSampleRate();
        return sampleRate > 0.0 ? sampleRate : 44100.0;
    }

private:
    MultiMeterAudioProcessor& audioProcessor;
    SpectrumEngine engine;

    // The most recent samples of both channels, which the FFTs analyze
    juce::AudioBuffer<float> analysisBuffer;

    // The ring buffer's sample count at the last analysis, to skip frames without new audio
    juce::uint64 lastTotalWritten = 0;
};
