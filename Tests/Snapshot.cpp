#include "../Source/PluginProcessor.h"
#include "../Source/UI/LookAndFeel.h"
#include <thread>

//==============================================================================
// A development tool that runs the real editor with a test signal, and saves a
// picture of it. It shows what the plugin looks like without a host.
//
//   GriseySnapshot <output.png> [view] [seconds] [parameterID=value ...]
//
// The views are 0 goniometer, 1 spectrum, 2 spectrogram, 3 history and 4 loudness.
// Any parameter can be set by its ID, for example spectrumChannels=1 or goniometerMode=1,
// and size=1400x800 sets the size of the editor.
//
// also=2,3 takes pictures of those views as well, of the same moment: the audio is stopped
// first, so that time stands still, and then each view is shown and saved in turn, as
// output-2.png and output-3.png. It shows what the views recorded while they were hidden.
//
// The test signal is a 440 Hz tone, a quieter 3 kHz tone that is out of phase between
// the channels, a tone that sweeps up from 200 Hz to 8 kHz every 4 s, and a little noise.
// The whole signal swells and fades every 8 s, so every meter has something to show, and
// there is a loud click every 5 s, which marks a moment that can be found in every view.
//
// click=Reset@20 presses the button of that name 20 s in, which is how a button can be tried
// without a hand on the mouse.
//
// A menu closes as soon as its application is not the one in front, so it cannot be opened for a
// picture. "GriseySnapshot menu.png menu" draws a sample menu with the look and feel instead: a
// section, a current choice, a submenu, a separator and an item that is switched off.
//
// audio=song.mp3 plays a file through the plugin instead of the test signal, in a loop, and
// from=30 starts it 30 s in. The file is read with whatever formats the system offers.
namespace
{
    // Draws the items of a sample menu one below the other, each at the size that the look and feel asks for
    juce::Image drawSampleMenu()
    {
        struct Item
        {
            juce::String text;
            bool isHeader = false, isSeparator = false, isActive = true, isHighlighted = false, isTicked = false, hasSubMenu = false;
        };

        const std::vector<Item> items {
            { "Level meters", true },
            { "Show", false, false, true, false, false, true },
            { "Peak ticks", true },
            { "Show ticks", false, false, true, false, true },
            { "Hold for", false, false, true, true, false, true },
            { "Then fall at", false, false, true, false, false, true },
            { "Reset ticks" },
            { {}, false, true },
            { "Switched off", false, false, false },
        };

        GriseyLookAndFeel lookAndFeel;
        const int border = lookAndFeel.getPopupMenuBorderSize();

        std::vector<juce::Rectangle<int>> areas;
        int width = 0, y = border;

        for (const auto& item : items)
        {
            int itemWidth = 0, itemHeight = 0;

            if (item.isHeader)
                lookAndFeel.getIdealPopupMenuSectionHeaderSizeWithOptions(item.text, -1, itemWidth, itemHeight, {});
            else
                lookAndFeel.getIdealPopupMenuItemSize(item.text, item.isSeparator, 0, itemWidth, itemHeight);

            areas.push_back({ border, y, itemWidth, itemHeight });
            width = juce::jmax(width, itemWidth);
            y += itemHeight;
        }

        juce::Image image(juce::Image::ARGB, 2 * (width + 2 * border), 2 * (y + border), true);
        juce::Graphics g(image);
        g.addTransform(juce::AffineTransform::scale(2.f));
        lookAndFeel.drawPopupMenuBackground(g, width + 2 * border, y + border);

        for (size_t i = 0; i < items.size(); ++i)
        {
            const auto& item = items[i];
            const auto area = areas[i].withWidth(width);

            if (item.isHeader)
                lookAndFeel.drawPopupMenuSectionHeader(g, area, item.text);
            else
                lookAndFeel.drawPopupMenuItem(g, area, item.isSeparator, item.isActive, item.isHighlighted, item.isTicked,
                                              item.hasSubMenu, item.text, {}, nullptr, nullptr);
        }

        return image;
    }
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cout << "Usage: GriseySnapshot <output.png> [view] [seconds] [parameterID=value ...]" << std::endl;
        return 1;
    }

    const juce::File output = juce::File::getCurrentWorkingDirectory().getChildFile(juce::String(argv[1]));
    const int view = argc > 2 ? juce::String(argv[2]).getIntValue() : 1;
    const double seconds = argc > 3 ? juce::String(argv[3]).getDoubleValue() : 2.0;

    juce::ScopedJuceInitialiser_GUI juceInit;

    if (argc > 2 && juce::String(argv[2]) == "menu")
    {
        output.deleteFile();
        juce::FileOutputStream stream(output);
        if (stream.openedOk())
            juce::PNGImageFormat().writeImageToStream(drawSampleMenu(), stream);

        std::cout << "Saved " << output.getFullPathName() << std::endl;
        return 0;
    }

    double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    // The file has to be read before the processor is prepared, because it decides the sample rate
    juce::AudioBuffer<float> audioFile;
    double audioFileStart = 0.0;

    for (int i = 4; i < argc; ++i)
    {
        const auto argument = juce::String(argv[i]);
        const auto value = argument.fromFirstOccurrenceOf("=", false, false);

        if (argument.startsWith("from="))
            audioFileStart = value.getDoubleValue();

        if (! argument.startsWith("audio="))
            continue;

        juce::AudioFormatManager formats;
        formats.registerBasicFormats();

        const std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(juce::File::getCurrentWorkingDirectory().getChildFile(value)));

        if (reader == nullptr || reader->lengthInSamples < blockSize)
        {
            std::cout << "Could not read " << value << std::endl;
            return 1;
        }

        sampleRate = reader->sampleRate;
        audioFile.setSize(2, (int) reader->lengthInSamples);
        reader->read(&audioFile, 0, (int) reader->lengthInSamples, 0, true, true);

        // A mono file plays on both channels
        if (reader->numChannels < 2)
            audioFile.copyFrom(1, 0, audioFile, 0, 0, audioFile.getNumSamples());
    }

    GriseyAudioProcessor processor;
    processor.setPlayConfigDetails(2, 2, sampleRate, blockSize);
    processor.prepareToPlay(sampleRate, blockSize);

    if (auto* parameter = processor.apvts.getParameter(Parameters::ID::mainView))
        parameter->setValueNotifyingHost(parameter->convertTo0to1((float) view));

    juce::String size, buttonToClick;
    double secondsUntilClick = 0.0;
    juce::Array<int> alsoViews;

    for (int i = 4; i < argc; ++i)
    {
        const auto argument = juce::String(argv[i]);
        const auto id = argument.upToFirstOccurrenceOf("=", false, false);

        if (id == "audio" || id == "from")
            continue;

        if (id == "click")
        {
            buttonToClick = argument.fromFirstOccurrenceOf("=", false, false).upToFirstOccurrenceOf("@", false, false);
            secondsUntilClick = argument.fromFirstOccurrenceOf("@", false, false).getDoubleValue();
        }
        else if (id == "size")
            size = argument.fromFirstOccurrenceOf("=", false, false);
        else if (id == "also")
            for (auto& token : juce::StringArray::fromTokens(argument.fromFirstOccurrenceOf("=", false, false), ",", {}))
                alsoViews.add(token.getIntValue());
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

        const int fileLength = audioFile.getNumSamples();
        const auto fileOffset = (juce::int64) (audioFileStart * sampleRate);

        while (running.load())
        {
            if (fileLength > 0)
            {
                for (int i = 0; i < blockSize; ++i, ++position)
                {
                    const int source = (int) ((fileOffset + position) % fileLength);
                    block.setSample(0, i, audioFile.getSample(0, source));
                    block.setSample(1, i, audioFile.getSample(1, source));
                }
            }
            else
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

                    // A burst of noise for 30 ms in every 5 s
                    const float click = std::fmod(t, 5.0) < 0.03 ? 0.8f * (random.nextFloat() * 2.f - 1.f) : 0.f;

                    block.setSample(0, i, swell * (low + high + sweep) + click + 0.01f * (random.nextFloat() - 0.5f));
                    block.setSample(1, i, swell * (0.7f * (low - high) + sweep) + click + 0.01f * (random.nextFloat() - 0.5f));
                }
            }

            processor.processBlock(block, midi);
            std::this_thread::sleep_until(start + std::chrono::duration<double>((double) position / sampleRate));
        }
    });

    auto save = [&](const juce::File& file)
    {
        const auto image = editor->createComponentSnapshot(editor->getLocalBounds(), true, 2.f);

        file.deleteFile();
        juce::FileOutputStream stream(file);
        if (stream.openedOk())
            juce::PNGImageFormat().writeImageToStream(image, stream);
    };

    // Shows each of the other views in turn and saves it, then stops the message loop.
    // A view needs a moment to be drawn after it has been shown.
    std::function<void(int)> saveOtherView = [&](int index)
    {
        if (index >= alsoViews.size())
        {
            juce::MessageManager::getInstance()->stopDispatchLoop();
            return;
        }

        if (auto* parameter = processor.apvts.getParameter(Parameters::ID::mainView))
            parameter->setValueNotifyingHost(parameter->convertTo0to1((float) alsoViews[index]));

        juce::Timer::callAfterDelay(400, [&, index]
        {
            save(output.getSiblingFile(output.getFileNameWithoutExtension() + "-" + juce::String(alsoViews[index]) + output.getFileExtension()));
            saveOtherView(index + 1);
        });
    };

    // Press the button that was asked for, when its time comes
    if (editor != nullptr && buttonToClick.isNotEmpty())
    {
        juce::Timer::callAfterDelay((int) (secondsUntilClick * 1000.0), [&]
        {
            for (auto* child : editor->getChildren())
                if (auto* button = dynamic_cast<juce::Button*>(child); button != nullptr && button->getName() == buttonToClick)
                    return button->triggerClick();

            std::cout << "There is no button called " << buttonToClick << std::endl;
        });
    }

    // Let the editor run for a while, then take the pictures
    juce::Timer::callAfterDelay((int) (seconds * 1000.0), [&]
    {
        if (editor == nullptr)
            return;

        if (alsoViews.isEmpty())
        {
            save(output);
            juce::MessageManager::getInstance()->stopDispatchLoop();
            return;
        }

        // Stop the audio, and wait until the editor has noticed, so that every picture is of the same moment
        running.store(false);
        juce::Timer::callAfterDelay(700, [&]
        {
            save(output);
            saveOtherView(0);
        });
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
