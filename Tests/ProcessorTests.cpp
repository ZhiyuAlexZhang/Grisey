#include "../Source/PluginProcessor.h"

namespace
{
    constexpr double testSampleRate = 48000.0;

    // Fills a stereo buffer with sines. rightPhase is the phase offset of the right channel in radians.
    juce::AudioBuffer<float> makeSine(int numSamples, float amplitude, float frequency, float rightPhase, float rightGain = 1.f)
    {
        juce::AudioBuffer<float> buffer(2, numSamples);
        for (int i = 0; i < numSamples; ++i)
        {
            const float phase = juce::MathConstants<float>::twoPi * frequency * (float) i / (float) testSampleRate;
            buffer.setSample(0, i, amplitude * std::sin(phase));
            buffer.setSample(1, i, amplitude * rightGain * std::sin(phase + rightPhase));
        }
        return buffer;
    }

    // Feeds a buffer to the engine in blocks, as a host would
    void processInBlocks(MeterEngine& engine, const juce::AudioBuffer<float>& buffer, int blockSize)
    {
        for (int start = 0; start < buffer.getNumSamples(); start += blockSize)
        {
            const int length = juce::jmin(blockSize, buffer.getNumSamples() - start);
            engine.process(buffer.getReadPointer(0, start), buffer.getReadPointer(1, start), length);
        }
    }
}

//==============================================================================
struct SampleRingBufferTests : juce::UnitTest
{
    SampleRingBufferTests() : juce::UnitTest("SampleRingBuffer") {}

    static void writeRamp(SampleRingBuffer& ring, int first, int count)
    {
        std::vector<float> left((size_t) count), right((size_t) count);
        for (int i = 0; i < count; ++i)
        {
            left[(size_t) i] = (float) (first + i);
            right[(size_t) i] = -(float) (first + i);
        }
        ring.write(left.data(), right.data(), count);
    }

    void runTest() override
    {
        beginTest("Reads the most recent samples, oldest first");
        {
            SampleRingBuffer ring;
            writeRamp(ring, 0, 1000);

            std::array<float, 16> left {}, right {};
            expect(ring.readLatest(left.data(), right.data(), 16));
            expectEquals(left[0], 984.f);
            expectEquals(left[15], 999.f);
            expectEquals(right[15], -999.f);
            expectEquals((int) ring.getTotalWritten(), 1000);
        }

        beginTest("Pads with silence when too few samples have been written");
        {
            SampleRingBuffer ring;
            writeRamp(ring, 1, 4);

            std::array<float, 8> left {}, right {};
            left.fill(99.f);
            expect(ring.readLatest(left.data(), right.data(), 8));
            expectEquals(left[3], 0.f);
            expectEquals(left[4], 1.f);
            expectEquals(left[7], 4.f);
        }

        beginTest("Stays in order across the wrap-around");
        {
            SampleRingBuffer ring;
            const int total = SampleRingBuffer::capacity + 5000;
            for (int written = 0; written < total; written += 777)
                writeRamp(ring, written, juce::jmin(777, total - written));

            std::vector<float> left(8192), right(8192);
            expect(ring.readLatest(left.data(), right.data(), 8192));

            bool inOrder = true;
            for (int i = 0; i < 8192; ++i)
                inOrder = inOrder && juce::exactlyEqual(left[(size_t) i], (float) (total - 8192 + i));
            expect(inOrder);
        }

        beginTest("A block longer than the buffer keeps its newest samples");
        {
            SampleRingBuffer ring;
            writeRamp(ring, 0, SampleRingBuffer::capacity + 100);

            std::array<float, 4> left {}, right {};
            expect(ring.readLatest(left.data(), right.data(), 4));
            expectEquals(left[3], (float) (SampleRingBuffer::capacity + 99));
        }

        beginTest("Reset forgets what was written");
        {
            SampleRingBuffer ring;
            writeRamp(ring, 1, 100);
            ring.reset();
            expectEquals((int) ring.getTotalWritten(), 0);

            std::array<float, 4> left {}, right {};
            left.fill(99.f);
            expect(ring.readLatest(left.data(), right.data(), 4));
            expectEquals(left[3], 0.f);
        }
    }
};

//==============================================================================
struct MeterEngineTests : juce::UnitTest
{
    MeterEngineTests() : juce::UnitTest("MeterEngine") {}

    void runTest() override
    {
        beginTest("Peak is the amplitude of a sine");
        {
            MeterEngine engine;
            engine.prepare(testSampleRate);
            processInBlocks(engine, makeSine(48000, 0.5f, 997.f, 0.f, 0.5f), 512);

            const auto readings = engine.read();
            expectWithinAbsoluteError(readings.peak[0], 0.5f, 0.001f);
            expectWithinAbsoluteError(readings.peak[1], 0.25f, 0.001f);
        }

        beginTest("Peak catches one sample in any block since the last read");
        {
            // Version 1 measured only the last block of each frame, and missed this
            MeterEngine engine;
            engine.prepare(testSampleRate);

            juce::AudioBuffer<float> silence(2, 512), spike(2, 512);
            silence.clear();
            spike.clear();
            spike.setSample(0, 200, 0.9f);

            processInBlocks(engine, spike, 512);
            for (int i = 0; i < 5; ++i)
                processInBlocks(engine, silence, 512);

            expectWithinAbsoluteError(engine.read().peak[0], 0.9f, 1.0e-6f);
        }

        beginTest("Reading restarts the peak measurement");
        {
            MeterEngine engine;
            engine.prepare(testSampleRate);
            processInBlocks(engine, makeSine(4800, 0.5f, 997.f, 0.f), 512);
            engine.read();
            expectEquals(engine.read().peak[0], 0.f);
        }

        beginTest("RMS of a sine is its amplitude over the square root of two");
        {
            MeterEngine engine;
            engine.prepare(testSampleRate);
            processInBlocks(engine, makeSine(48000, 0.5f, 1000.f, 0.f, 0.5f), 441);

            const auto readings = engine.read();
            expectWithinAbsoluteError(readings.rms[0], 0.5f / std::sqrt(2.f), 0.001f);
            expectWithinAbsoluteError(readings.rms[1], 0.25f / std::sqrt(2.f), 0.001f);
        }

        beginTest("RMS of a full-scale square wave is one");
        {
            MeterEngine engine;
            engine.prepare(testSampleRate);

            juce::AudioBuffer<float> square(2, 48000);
            for (int i = 0; i < square.getNumSamples(); ++i)
            {
                square.setSample(0, i, (i / 24) % 2 == 0 ? 1.f : -1.f);
                square.setSample(1, i, square.getSample(0, i));
            }
            processInBlocks(engine, square, 512);

            expectWithinAbsoluteError(engine.read().rms[0], 1.f, 1.0e-4f);
        }

        beginTest("RMS falls to zero once the window holds only silence");
        {
            MeterEngine engine;
            engine.prepare(testSampleRate);
            processInBlocks(engine, makeSine(48000, 0.5f, 1000.f, 0.f), 512);

            juce::AudioBuffer<float> silence(2, (int) (testSampleRate * MeterEngine::rmsWindowSeconds) + 512);
            silence.clear();
            processInBlocks(engine, silence, 512);

            expectWithinAbsoluteError(engine.read().rms[0], 0.f, 1.0e-6f);
        }

        beginTest("RMS stays accurate over a long run");
        {
            // Ten minutes of audio, to show that the running sum does not drift
            MeterEngine engine;
            engine.prepare(testSampleRate);
            const auto second = makeSine(48000, 0.1f, 1000.f, 0.f);
            for (int i = 0; i < 600; ++i)
                processInBlocks(engine, second, 512);

            expectWithinAbsoluteError(engine.read().rms[0], 0.1f / std::sqrt(2.f), 1.0e-4f);
        }

        beginTest("Correlation of identical channels is +1");
        {
            MeterEngine engine;
            engine.prepare(testSampleRate);
            engine.setSlowCorrelationSeconds(0.1f);
            processInBlocks(engine, makeSine(48000, 0.5f, 440.f, 0.f, 0.3f), 512);

            const auto readings = engine.read();
            expectWithinAbsoluteError(readings.correlationFast, 1.f, 0.001f);
            expectWithinAbsoluteError(readings.correlationSlow, 1.f, 0.001f);
        }

        beginTest("Correlation of inverted channels is -1");
        {
            MeterEngine engine;
            engine.prepare(testSampleRate);
            processInBlocks(engine, makeSine(48000, 0.5f, 440.f, juce::MathConstants<float>::pi), 512);

            const auto readings = engine.read();
            expectWithinAbsoluteError(readings.correlationFast, -1.f, 0.001f);
            expectWithinAbsoluteError(readings.correlationSlow, -1.f, 0.001f);
        }

        beginTest("Correlation of channels in quadrature is 0");
        {
            MeterEngine engine;
            engine.prepare(testSampleRate);
            engine.setSlowCorrelationSeconds(1.f);
            processInBlocks(engine, makeSine(48000 * 5, 0.5f, 440.f, juce::MathConstants<float>::halfPi), 512);

            expectWithinAbsoluteError(engine.read().correlationSlow, 0.f, 0.01f);
        }

        beginTest("Correlation of silence is 0");
        {
            MeterEngine engine;
            engine.prepare(testSampleRate);

            juce::AudioBuffer<float> silence(2, 4800);
            silence.clear();
            processInBlocks(engine, silence, 512);

            const auto readings = engine.read();
            expectEquals(readings.correlationFast, 0.f);
            expectEquals(readings.correlationSlow, 0.f);
        }

        beginTest("The slow correlation follows its integration time");
        {
            // After the signal flips from in phase to out of phase, a short
            // integration time has followed and a long one has not
            auto correlationAfterFlip = [](float seconds)
            {
                MeterEngine engine;
                engine.prepare(testSampleRate);
                engine.setSlowCorrelationSeconds(seconds);
                processInBlocks(engine, makeSine(48000 * 4, 0.5f, 440.f, 0.f), 512);
                processInBlocks(engine, makeSine(14400, 0.5f, 440.f, juce::MathConstants<float>::pi), 512);
                return engine.read().correlationSlow;
            };

            expect(correlationAfterFlip(0.1f) < -0.8f);
            expect(correlationAfterFlip(2.f) > 0.5f);
        }
    }
};

//==============================================================================
struct ProcessorTests : juce::UnitTest
{
    ProcessorTests() : juce::UnitTest("Processor") {}

    static float getValue(GriseyAudioProcessor& processor, const juce::String& parameterID)
    {
        return processor.apvts.getRawParameterValue(parameterID)->load();
    }

    static void setValue(GriseyAudioProcessor& processor, const juce::String& parameterID, float value)
    {
        auto* parameter = processor.apvts.getParameter(parameterID);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    }

    void runTest() override
    {
        juce::MidiBuffer midi;

        beginTest("processBlock measures the block and passes it through unchanged");
        {
            GriseyAudioProcessor processor;
            processor.setPlayConfigDetails(2, 2, testSampleRate, 512);
            processor.prepareToPlay(testSampleRate, 512);

            auto block = makeSine(512, 0.5f, 1000.f, juce::MathConstants<float>::pi);
            const auto original = block;
            processor.processBlock(block, midi);

            expectWithinAbsoluteError(processor.meterEngine.read().peak[0], 0.5f, 0.01f);
            expectEquals((int) processor.sampleRingBuffer.getTotalWritten(), 512);

            std::array<float, 4> left {}, right {};
            expect(processor.sampleRingBuffer.readLatest(left.data(), right.data(), 4));
            expectEquals(left[3], original.getSample(0, 511));
            expectEquals(right[3], original.getSample(1, 511));

            bool unchanged = true;
            for (int i = 0; i < 512; ++i)
                unchanged = unchanged && juce::exactlyEqual(block.getSample(0, i), original.getSample(0, i));
            expect(unchanged);
        }

        beginTest("A mono layout is analyzed as dual mono");
        {
            GriseyAudioProcessor processor;
            juce::AudioProcessor::BusesLayout layout;
            layout.inputBuses.add(juce::AudioChannelSet::mono());
            layout.outputBuses.add(juce::AudioChannelSet::mono());
            expect(processor.setBusesLayout(layout));
            processor.setRateAndBufferSizeDetails(testSampleRate, 512);
            processor.prepareToPlay(testSampleRate, 512);

            juce::AudioBuffer<float> block(1, 512);
            for (int i = 0; i < 512; ++i)
                block.setSample(0, i, 0.75f * std::sin((float) i * 0.1f));

            for (int i = 0; i < 20; ++i)
                processor.processBlock(block, midi);

            const auto readings = processor.meterEngine.read();
            expectWithinAbsoluteError(readings.peak[0], 0.75f, 0.01f);
            expectWithinAbsoluteError(readings.peak[1], 0.75f, 0.01f);
            expectWithinAbsoluteError(readings.correlationFast, 1.f, 0.001f);
        }

        beginTest("Blocks of any size are accepted, including empty ones");
        {
            GriseyAudioProcessor processor;
            processor.setPlayConfigDetails(2, 2, testSampleRate, 512);
            processor.prepareToPlay(testSampleRate, 512);

            for (int size : { 0, 1, 64, 512, 4096, 100000 })
            {
                juce::AudioBuffer<float> block(2, size);
                block.clear();
                processor.processBlock(block, midi);
            }

            expectEquals((int) processor.sampleRingBuffer.getTotalWritten(), 1 + 64 + 512 + 4096 + 100000);
        }

        beginTest("Settings survive a save and a load");
        {
            GriseyAudioProcessor saved;
            setValue(saved, Parameters::ID::goniometerScale, 150.f);
            setValue(saved, Parameters::ID::decayRate, 3.f);
            setValue(saved, Parameters::ID::holdTime, 5.f);
            setValue(saved, Parameters::ID::averagerDuration, 4.f);
            setValue(saved, Parameters::ID::meterView, 2.f);
            setValue(saved, Parameters::ID::mainView, 0.f);
            setValue(saved, Parameters::ID::showTick, 0.f);

            juce::MemoryBlock state;
            saved.getStateInformation(state);

            GriseyAudioProcessor loaded;
            loaded.setStateInformation(state.getData(), (int) state.getSize());

            expectEquals(getValue(loaded, Parameters::ID::goniometerScale), 150.f);
            expectEquals(getValue(loaded, Parameters::ID::decayRate), 3.f);
            expectEquals(getValue(loaded, Parameters::ID::holdTime), 5.f);
            expectEquals(getValue(loaded, Parameters::ID::averagerDuration), 4.f);
            expectEquals(getValue(loaded, Parameters::ID::meterView), 2.f);
            expectEquals(getValue(loaded, Parameters::ID::mainView), 0.f);
            expectEquals(getValue(loaded, Parameters::ID::showTick), 0.f);
        }

        beginTest("The saved state carries its version number");
        {
            GriseyAudioProcessor processor;
            juce::MemoryBlock state;
            processor.getStateInformation(state);

            auto xml = juce::AudioProcessor::getXmlFromBinary(state.getData(), (int) state.getSize());
            expect(xml != nullptr);
            if (xml != nullptr)
                expectEquals(xml->getIntAttribute(Parameters::stateVersionProperty.toString()), Parameters::currentStateVersion);
        }

        beginTest("A session saved by version 1 loads its settings");
        {
            // This is the stream that version 1 wrote in getStateInformation()
            juce::MemoryBlock legacy;
            {
                juce::MemoryOutputStream stream(legacy, false);
                stream.writeFloat(125.f); // sliderValue
                stream.writeInt(4);       // levelMeterDecayId, -24dB/s
                stream.writeInt(6);       // holdTimeId, inf
                stream.writeBool(false);  // tickDisplayState
                stream.writeInt(3);       // averagerDurationId, 500ms
                stream.writeInt(1);       // levelMeterDisplayID, Peak
                stream.writeInt(1);       // histogramDisplayID, Stacked
            }
            expectEquals((int) legacy.getSize(), Parameters::legacyStateSizeInBytes);

            GriseyAudioProcessor processor;
            processor.setStateInformation(legacy.getData(), (int) legacy.getSize());

            expectEquals(getValue(processor, Parameters::ID::goniometerScale), 125.f);
            expectEquals(getValue(processor, Parameters::ID::decayRate), 3.f);
            expectEquals(getValue(processor, Parameters::ID::holdTime), 5.f);
            expectEquals(getValue(processor, Parameters::ID::showTick), 0.f);
            expectEquals(getValue(processor, Parameters::ID::averagerDuration), 2.f);
            expectEquals(getValue(processor, Parameters::ID::meterView), 1.f);
        }

        beginTest("A version 1 session with uninitialized values falls back to the defaults");
        {
            juce::MemoryBlock legacy;
            {
                juce::MemoryOutputStream stream(legacy, false);
                stream.writeFloat(-3.0e12f);
                stream.writeInt(-559038737);
                stream.writeInt(77);
                stream.writeBool(true);
                stream.writeInt(0);
                stream.writeInt(9);
                stream.writeInt(-1);
            }

            GriseyAudioProcessor processor;
            processor.setStateInformation(legacy.getData(), (int) legacy.getSize());

            expectEquals(getValue(processor, Parameters::ID::goniometerScale), 100.f);
            expectEquals(getValue(processor, Parameters::ID::decayRate), 0.f);
            expectEquals(getValue(processor, Parameters::ID::holdTime), 2.f);
            expectEquals(getValue(processor, Parameters::ID::averagerDuration), 0.f);
            expectEquals(getValue(processor, Parameters::ID::meterView), 0.f);
        }

        beginTest("Unreadable state is ignored");
        {
            GriseyAudioProcessor processor;
            setValue(processor, Parameters::ID::decayRate, 2.f);

            const std::array<char, 7> garbage { 'g', 'a', 'r', 'b', 'a', 'g', 'e' };
            processor.setStateInformation(garbage.data(), (int) garbage.size());
            processor.setStateInformation(nullptr, 0);

            expectEquals(getValue(processor, Parameters::ID::decayRate), 2.f);
        }
    }
};

static SampleRingBufferTests sampleRingBufferTests;
static MeterEngineTests meterEngineTests;
static ProcessorTests processorTests;
