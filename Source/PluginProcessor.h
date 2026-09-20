/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.
    This project is built with JUCE version 9.

  ==============================================================================
*/

#pragma once

// Macro used for testing.
#define USE_OSC false

#include <JuceHeader.h>
#include <array>

using namespace juce;

using Filter = juce::dsp::IIR::Filter<float>;
using CutFilter = juce::dsp::ProcessorChain<Filter, Filter, Filter, Filter>;
using MonoChain = juce::dsp::ProcessorChain<CutFilter, Filter, Filter, Filter, CutFilter>;

//==============================================================================
// A standard AbstractFifo-based templated FIFO class
template<typename T, size_t Size>
struct Fifo
{
    // Returns the fixed size of the FIFO buffer.
    size_t getSize() const noexcept { return Size; }

    // Prepares the FIFO buffer for writing with the given number of samples and channels
    void prepare(int numSamples, int numChannels)
    {
        for (auto& buffer : buffers)
        {
            buffer.setSize(numChannels,
                numSamples,
                false, // clearUnusedChannels
                true,  // allocateExtraSpace
                true); // avoidReallocating
            buffer.clear();
        }
    }

    // Pushes an element into the FIFO buffer
    bool push(const T& t)
    {
        auto write = fifo.write(1);
        if (write.blockSize1 > 0)
        {
            copyInto(buffers[write.startIndex1], t);
            return true;
        }
        return false;
    }

    // Pulls an element from the FIFO buffer
    bool pull(T& t)
    {
        auto read = fifo.read(1);
        if (read.blockSize1 > 0)
        {
            t = buffers[read.startIndex1];
            return true;
        }
        return false;
    }

    // Returns the number of elements available for reading from the FIFO buffer
    int getNumAvailableForReading() const
    {
        return fifo.getNumReady();
    }

    // Returns the available space in the FIFO buffer for writing
    int getAvailableSpace() const
    {
        return fifo.getFreeSpace();
    }

private:
    // push() runs on the audio thread, so audio buffers are copied into the
    // preallocated slot without reallocating it
    template<typename SampleType>
    static void copyInto(juce::AudioBuffer<SampleType>& slot, const juce::AudioBuffer<SampleType>& source)
    {
        slot.setSize(source.getNumChannels(), source.getNumSamples(), false, false, true);

        for (int channel = 0; channel < source.getNumChannels(); ++channel)
            slot.copyFrom(channel, 0, source, channel, 0, source.getNumSamples());
    }

    template<typename U>
    static void copyInto(U& slot, const U& source) { slot = source; }

    juce::AbstractFifo fifo{Size}; // AbstractFifo object to manage buffer read/write positions
    std::array<T, Size> buffers; // Array of buffers to store data elements
};

//==============================================================================
template<typename T>
struct FifoSpectrumAnalyzer
{
    // Prepares the analyzer with the given number of channels and samples per channel
    void prepare(int numChannels, int numSamples)
    {
        for (auto& buffer : buffers)
        {
            buffer.setSize(numChannels, numSamples, false, true, true);
            buffer.clear();
        }
    }

    // Prepares the analyzer with the specified number of elements for each channel
    void prepare(size_t numElements)
    {
        for (auto& buffer : buffers)
        {
            buffer.clear();
            buffer.resize(numElements, 0);
        }
    }

    // Pushes an element into the analyzer's FIFO buffer
    bool push(const T& t)
    {
        auto write = fifo.write(1);
        if (write.blockSize1 > 0)
        {
            buffers[write.startIndex1] = t;
            return true;
        }

        return false;
    }

    // Pulls an element from the analyzer's FIFO buffer
    bool pull(T& t)
    {
        auto read = fifo.read(1);
        if (read.blockSize1 > 0)
        {
            t = buffers[read.startIndex1];
            return true;
        }

        return false;
    }

    // Returns the number of elements available for reading from the FIFO buffer
    int getNumAvailableForReading() const
    {
        return fifo.getNumReady();
    }

private:
    static constexpr int Capacity = 30; // Capacity of the FIFO buffer
    std::array<T, Capacity> buffers;    // Array of buffers to store data elements
    juce::AbstractFifo fifo {Capacity};  // AbstractFifo object to manage buffer read/write positions
};


//==============================================================================
enum Channel
{
    Left,
    Right,
};

//==============================================================================
template<typename BlockType>
struct SingleChannelSampleFifo
{
    // Constructor.
    SingleChannelSampleFifo(Channel ch) : channelToUse(ch)
    {
        prepared.set(false); // Initialize the prepared flag to false
    }

    // Updates the FIFO with new samples from the provided block
    void update(const BlockType& buffer)
    {
        jassert(prepared.get()); // Ensure that the FIFO is prepared.
        jassert(buffer.getNumChannels() > channelToUse); // Ensure that the buffer has the required number of channels
        auto* channelPtr = buffer.getReadPointer(channelToUse); // Get a pointer to the channel data

        // Iterate over the samples in the buffer and push them into the FIFO
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            pushNextSampleIntoFifo(channelPtr[i]);
        }
    }

    // Prepares the FIFO with the specified buffer size
    void prepare(int bufferSize)
    {
        prepared.set(false); // Mark the FIFO as not prepared
        size.set(bufferSize); // Set the size of the buffer

        bufferToFill.setSize(1, bufferSize, false, true, true); // Set the size of the temporary buffer
        audioBufferFifo.prepare(1, bufferSize); // Prepare the FIFO
        fifoIndex = 0; // Reset the FIFO index
        prepared.set(true); // Mark the FIFO as prepared
    }

    // Returns the number of complete buffers available for reading from the FIFO
    int getNumCompleteBuffersAvailable() const
    {
        return audioBufferFifo.getNumAvailableForReading();
    }

    // Checks if the FIFO is prepared
    bool isPrepared() const
    {
        return prepared.get();
    }

    // Returns the size of the FIFO buffer
    int getSize() const
    {
        return size.get();
    }

    // Retrieves a complete audio buffer from the FIFO
    bool getAudioBuffer(BlockType& buf)
    {
        return audioBufferFifo.pull(buf);
    }

private:
    Channel channelToUse; // The channel to use for the FIFO
    int fifoIndex = 0; // Index for managing the FIFO buffer
    FifoSpectrumAnalyzer<BlockType> audioBufferFifo; // FIFO for storing audio buffers
    BlockType bufferToFill; // Temporary buffer for filling the FIFO
    juce::Atomic<bool> prepared = false; // Flag indicating if the FIFO is prepared
    juce::Atomic<int> size = 0; // Size of the FIFO buffer

    // Pushes the next sample into the FIFO buffer
    void pushNextSampleIntoFifo(float sample)
    {
        // If the FIFO buffer is full, push the buffer into the FIFO
        if (fifoIndex == bufferToFill.getNumSamples())
        {
            auto ok = audioBufferFifo.push(bufferToFill);
            juce::ignoreUnused(ok); // Ignore the return value
            fifoIndex = 0; // Reset the FIFO index
        }

        bufferToFill.setSample(0, fifoIndex, sample); // Set the sample in the temporary buffer
        ++fifoIndex; // Move to the next index in the FIFO buffer
    }
};

//==============================================================================
class MultiMeterAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    MultiMeterAudioProcessor();
    ~MultiMeterAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    // Creates the parameter layout for the audio processor
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Manages the state of all parameters in the audio processor
    juce::AudioProcessorValueTreeState apvts;

    // Defines the block type for the FIFO
    using BlockType = juce::AudioBuffer<float>;

    // FIFO for storing samples from the left channel
    SingleChannelSampleFifo<BlockType> leftChannelFifo { Channel::Left };

    // FIFO for storing samples from the right channel
    SingleChannelSampleFifo<BlockType> rightChannelFifo { Channel::Right };

    // FIFO for storing audio buffers
    Fifo<juce::AudioBuffer<float>, 30> fifo;

    // Stereo copy of the incoming block that feeds the meters, so that mono
    // layouts can be analyzed the same way as stereo ones
    juce::AudioBuffer<float> analysisBuffer;

    // Value of the slider
    float sliderValue = 100.f;

    // Flag indicating the display state of ticks
    bool tickDisplayState = true;

    // IDs for various parameters
    int levelMeterDecayId = 1, holdTimeId = 3, averagerDurationId = 1, levelMeterDisplayID = 0, histogramDisplayID = 0;

#if USE_OSC
    // Oscillator for generating test signals
    juce::dsp::Oscillator<float> osc {[](float x) { return std::sin(x); }};

    // Gain control for the oscillator output
    juce::dsp::Gain<float> gain;
#endif

    
private:

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultiMeterAudioProcessor)
};
