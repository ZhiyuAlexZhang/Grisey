#include "../Source/PluginProcessor.h"
#include <thread>

//==============================================================================
// A development tool that runs the real editor with a test signal, and saves a
// picture of it. It shows what the plugin looks like without a host.
//
//   MultiMeterSnapshot <output.png> [view] [seconds] [parameterID=value ...]
//
// The views are 0 goniometer, 1 analyzer, 2 spectrogram, 3 histogram and 4 loudness.
// Any parameter can be set by its ID, for example spectrumChannels=1 or goniometerMode=1,
// and size=1400x800 sets the size of the editor.
//
// The test signal is a 440 Hz tone, a quieter 3 kHz tone that is out of phase between
// the channels, a tone that sweeps up from 200 Hz to 8 kHz every 4 s, and a little noise.
// The whole signal swells and fades every 8 s, so every meter has something to show.
int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cout << "Usage: MultiMeterSnapshot <output.png> [view] [seconds] [parameterID=value ...]" << std::endl;
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

    juce::String size;

    for (int i = 4; i < argc; ++i)
    {
        const auto argument = juce::String(argv[i]);
        const auto id = argument.upToFirstOccurrenceOf("=", false, false);

        if (id == "size")
            size = argument.fromFirstOccurrenceOf("=", false, false);
        else if (auto* parameter = processor.apvts.getParameter(id))
            parameter->setValueNotifyingHost(parameter->convertTo0to1(argument.fromFirstOccurrenceOf("=", false, false).getFloatValue()));
        else
            std::cout << "There is no parameter called " << id << std::endl;
    }

    // View -1 runs the signal without an editor, which shows how much of the CPU time is the tool's own
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    if (view >= 0)
    {
        editor.reset(processor.createEditorAndMakeActive());
        // The window appears wherever the user happens to be working, so it lets their clicks through
        // to what is underneath, rather than taking them as clicks on its tabs
        editor->addToDesktop(juce::ComponentPeer::windowIgnoresMouseClicks);
        editor->setVisible(true);

        if (size.isNotEmpty())
            editor->setSize(size.upToFirstOccurrenceOf("x", false, false).getIntValue(), size.fromFirstOccurrenceOf("x", false, false).getIntValue());
    }

    // Stands in for the host's audio thread, and delivers blocks in real time
    std::atomic<bool> running { true };
    std::thread audioThread([&]
    {
        juce::AudioBuffer<float> block(2, blockSize);
        juce::MidiBuffer midi;
        juce::Random random(1);
        juce::int64 position = 0;
        double sweepPhase = 0.0;
        const auto start = std::chrono::steady_clock::now();

        while (running.load())
        {
            for (int i = 0; i < blockSize; ++i, ++position)
            {
                const double t = (double) position / sampleRate;
                const float low = 0.4f * (float) std::sin(juce::MathConstants<double>::twoPi * 440.0 * t);
                const float high = 0.15f * (float) std::sin(juce::MathConstants<double>::twoPi * 3000.0 * t);

                // The sweep rises by the same ratio in every moment, which is a straight line on the spectrogram
                const double sweepFrequency = 200.0 * std::pow(40.0, std::fmod(t, 4.0) / 4.0);
                sweepPhase += juce::MathConstants<double>::twoPi * sweepFrequency / sampleRate;
                const float sweep = 0.1f * (float) std::sin(sweepPhase);

                const float swell = 0.55f + 0.45f * (float) std::sin(juce::MathConstants<double>::twoPi * t / 8.0);
                block.setSample(0, i, swell * (low + high + sweep) + 0.01f * (random.nextFloat() - 0.5f));
                block.setSample(1, i, swell * (0.7f * (low - high) + sweep) + 0.01f * (random.nextFloat() - 0.5f));
            }

            processor.processBlock(block, midi);
            std::this_thread::sleep_until(start + std::chrono::duration<double>((double) position / sampleRate));
        }
    });

    // Let the editor run for a while, then take the picture
    juce::Timer::callAfterDelay((int) (seconds * 1000.0), [&]
    {
        if (editor != nullptr)
        {
            const auto image = editor->createComponentSnapshot(editor->getLocalBounds(), true, 2.f);

            output.deleteFile();
            juce::FileOutputStream stream(output);
            if (stream.openedOk())
                juce::PNGImageFormat().writeImageToStream(image, stream);
        }

        juce::MessageManager::getInstance()->stopDispatchLoop();
    });

    // Without a window there is nothing for the message loop to wait on
    if (editor != nullptr)
        juce::MessageManager::getInstance()->runDispatchLoop();
    else
        std::this_thread::sleep_for(std::chrono::duration<double>(seconds));

    running.store(false);
    audioThread.join();
    editor.reset();

    std::cout << "Saved " << output.getFullPathName() << std::endl;
    return 0;
}
