#include "../Source/PluginProcessor.h"

//==============================================================================
// Checks that audio pushed by processBlock() reaches the GUI-side FIFO intact.
struct FifoTests : juce::UnitTest
{
    FifoTests() : juce::UnitTest("Fifo") {}

    void runTest() override
    {
        beginTest("Empty FIFO has nothing to pull");
        {
            Fifo<juce::AudioBuffer<float>, 30> fifo;
            fifo.prepare(64, 2);
            juce::AudioBuffer<float> out;
            expectEquals(fifo.getNumAvailableForReading(), 0);
            expect(! fifo.pull(out));
        }

        beginTest("Buffers come out in the order they went in");
        {
            Fifo<juce::AudioBuffer<float>, 30> fifo;
            fifo.prepare(64, 2);
            juce::AudioBuffer<float> in(2, 64), out;

            for (int i = 1; i <= 3; ++i)
            {
                in.clear();
                in.setSample(0, 0, (float) i);
                expect(fifo.push(in));
            }

            expectEquals(fifo.getNumAvailableForReading(), 3);

            for (int i = 1; i <= 3; ++i)
            {
                expect(fifo.pull(out));
                expectEquals(out.getSample(0, 0), (float) i);
            }

            expect(! fifo.pull(out));
        }

        beginTest("Full FIFO rejects pushes");
        {
            Fifo<juce::AudioBuffer<float>, 30> fifo;
            fifo.prepare(64, 2);
            juce::AudioBuffer<float> in(2, 64);
            in.clear();

            int accepted = 0;
            for (int i = 0; i < 40; ++i)
                accepted += fifo.push(in) ? 1 : 0;

            expect(accepted < 40);
            expectEquals(fifo.getNumAvailableForReading(), accepted);
        }
    }
};

//==============================================================================
struct ProcessorTests : juce::UnitTest
{
    ProcessorTests() : juce::UnitTest("Processor") {}

    void runTest() override
    {
        beginTest("processBlock makes the block available to the editor");

        MultiMeterAudioProcessor processor;
        processor.setPlayConfigDetails(2, 2, 48000.0, 512);
        processor.prepareToPlay(48000.0, 512);

        juce::AudioBuffer<float> block(2, 512);
        for (int i = 0; i < block.getNumSamples(); ++i)
        {
            auto s = 0.5f * std::sin(juce::MathConstants<float>::twoPi * 1000.f * (float) i / 48000.f);
            block.setSample(0, i, s);
            block.setSample(1, i, -s);
        }

        juce::MidiBuffer midi;
        processor.processBlock(block, midi);

        juce::AudioBuffer<float> pulled;
        expectEquals(processor.fifo.getNumAvailableForReading(), 1);
        expect(processor.fifo.pull(pulled));
        expectEquals(pulled.getNumSamples(), 512);
        expectWithinAbsoluteError(pulled.getMagnitude(0, 0, 512), 0.5f, 0.01f);
        expectWithinAbsoluteError(pulled.getSample(1, 100), -pulled.getSample(0, 100), 1.0e-6f);

        beginTest("Blocks shorter than the prepared size keep their length");
        {
            juce::AudioBuffer<float> shortBlock(2, 64);
            shortBlock.clear();
            shortBlock.setSample(0, 63, 0.25f);
            processor.processBlock(shortBlock, midi);

            expect(processor.fifo.pull(pulled));
            expectEquals(pulled.getNumSamples(), 64);
            expectEquals(pulled.getSample(0, 63), 0.25f);
        }

        beginTest("A mono layout is analyzed as dual mono");
        {
            MultiMeterAudioProcessor mono;
            juce::AudioProcessor::BusesLayout layout;
            layout.inputBuses.add(juce::AudioChannelSet::mono());
            layout.outputBuses.add(juce::AudioChannelSet::mono());
            expect(mono.setBusesLayout(layout));
            mono.setRateAndBufferSizeDetails(48000.0, 512);
            mono.prepareToPlay(48000.0, 512);

            juce::AudioBuffer<float> monoBlock(1, 512);
            monoBlock.clear();
            monoBlock.setSample(0, 10, 0.75f);
            mono.processBlock(monoBlock, midi);

            expect(mono.fifo.pull(pulled));
            expectEquals(pulled.getNumChannels(), 2);
            expectEquals(pulled.getSample(0, 10), 0.75f);
            expectEquals(pulled.getSample(1, 10), 0.75f);
        }
    }
};

static FifoTests fifoTests;
static ProcessorTests processorTests;

//==============================================================================
int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure(false);
    runner.runAllTests();

    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        failures += runner.getResult(i)->failures;

    return failures == 0 ? 0 : 1;
}
