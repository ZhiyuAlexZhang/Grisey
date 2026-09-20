#include "../Source/PluginProcessor.h"
#include <thread>

//==============================================================================
// A development tool that runs the real editor with a test signal, and saves a
// picture of it. It shows what the plugin looks like without a host.
//
//   MultiMeterSnapshot <output.png> [view: 0 goniometer, 1 analyzer, 2 histogram] [seconds]
//
// The test signal is a 440 Hz tone, a quieter 3 kHz tone that is out of phase
// between the channels, and a little noise, so every meter has something to show.
int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cout << "Usage: MultiMeterSnapshot <output.png> [view] [seconds]" << std::endl;
        return 1;
    }

    const juce::File output = juce::File::getCurrentWorkingDirectory().getChildFile(juce::String(argv[1]));
    const int view = argc > 2 ? juce::String(argv[2]).getIntValue() : 1;
    const double seconds = argc > 3 ? juce::String(argv[3]).getDoubleValue() : 2.0;

    juce::ScopedJuceInitialiser_GUI juceInit;

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    MultiMeterAudioProcessor processor;
    processor.setPlayConfigDetails(2, 2, sampleRate, blockSize);
    processor.prepareToPlay(sampleRate, blockSize);

    if (auto* parameter = processor.apvts.getParameter(Parameters::ID::mainView))
        parameter->setValueNotifyingHost(parameter->convertTo0to1((float) view));

    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditorAndMakeActive());
    editor->addToDesktop(0);
    editor->setVisible(true);

    // Stands in for the host's audio thread, and delivers blocks in real time
    std::atomic<bool> running { true };
    std::thread audioThread([&]
    {
        juce::AudioBuffer<float> block(2, blockSize);
        juce::MidiBuffer midi;
        juce::Random random(1);
        juce::int64 position = 0;
        const auto start = std::chrono::steady_clock::now();

        while (running.load())
        {
            for (int i = 0; i < blockSize; ++i, ++position)
            {
                const double t = (double) position / sampleRate;
                const float low = 0.4f * (float) std::sin(juce::MathConstants<double>::twoPi * 440.0 * t);
                const float high = 0.15f * (float) std::sin(juce::MathConstants<double>::twoPi * 3000.0 * t);
                block.setSample(0, i, low + high + 0.01f * (random.nextFloat() - 0.5f));
                block.setSample(1, i, 0.7f * (low - high) + 0.01f * (random.nextFloat() - 0.5f));
            }

            processor.processBlock(block, midi);
            std::this_thread::sleep_until(start + std::chrono::duration<double>((double) position / sampleRate));
        }
    });

    // Let the editor run for a while, then take the picture
    juce::Timer::callAfterDelay((int) (seconds * 1000.0), [&]
    {
        const auto image = editor->createComponentSnapshot(editor->getLocalBounds(), true, 2.f);

        output.deleteFile();
        juce::FileOutputStream stream(output);
        if (stream.openedOk())
            juce::PNGImageFormat().writeImageToStream(image, stream);

        juce::MessageManager::getInstance()->stopDispatchLoop();
    });

    juce::MessageManager::getInstance()->runDispatchLoop();

    running.store(false);
    audioThread.join();
    editor.reset();

    std::cout << "Saved " << output.getFullPathName() << std::endl;
    return 0;
}
