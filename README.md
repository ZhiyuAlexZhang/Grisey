# MultiMeter

## Overview

MultiMeter, a cutting-edge AU/VST3/CLAP audio analyzer, caters to audio engineers, producers, and musicians who seek precision and versatility. Leveraging the JUCE framework, MultiMeter delivers a robust array of features for pristine real-time audio analysis, enhancing mixing, mastering, and sound design processes.

![multimeter-demo](https://github.com/RealAlexZ/MultiMeter/assets/97690118/ce64ecb6-801e-4e9d-8815-1f97655c272d)

## Features

### General Metering
- Employs a high-performance FIFO (First In, First Out) buffer to handle audio data between DSP and GUI threads.
- Features comboboxes and sliders to personalize metering behavior.

### Level Meter
- Provides instantaneous visual feedback of audio signal levels with numeric value displays in decibels.
- Supports both Root Mean Squared (RMS) and peak readings.
- Enables user adjustment of the decay rate of meter ticks with multiple responsiveness options.
- Allows holding peak tick values for a specified duration to enhance the analysis of transient audio materials.

### FFT Spectrogram Analyzer
- Presents a high-resolution Fast Fourier Transform (FFT) spectrum with logarithmically scaled frequency bins, displaying the frequency content over time with a curve of all frequency components in the incoming signal and enabling in-depth spectral balance analysis.

### Histogram
- Visualizes the distribution of signal level dynamics over time.

### Correlation Meter
- Provides instantaneous and average readings of the phase correlation between left and right audio channels, ranging from +1 (fully in-phase) to 0 (wide stereo) to -1 (out-of-phase), for identifying phase issues and ensuring mono compatibility.

### Goniometer
- Converts L/R audio signals into Mid/Side representations that provide insights into the coherence of the stereo field distribution and phase differences between the left and right channels.

## Building

MultiMeter builds with CMake 3.22 or later and a C++20 compiler. JUCE 9 and
clap-juce-extensions are included as git submodules.

```bash
git clone --recurse-submodules https://github.com/RealAlexZ/MultiMeter.git
cd MultiMeter
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

The VST3, AU (macOS only), CLAP, and Standalone builds are written to
`build/MultiMeter_artefacts/Release`. Add `-DMULTIMETER_COPY_AFTER_BUILD=ON` to install them
into your user plugin folders as part of the build.

To run the unit tests:

```bash
ctest --test-dir build -C Release --output-on-failure
```

To save a picture of the editor running a test signal, without opening a host:

```bash
build/MultiMeterSnapshot_artefacts/Release/MultiMeterSnapshot editor.png
```

## Roadmap

Version 2.0 is in progress. See [ROADMAP.md](ROADMAP.md) for the plan.

## Dependencies
- **JUCE:** 9.0.2
- **clap-juce-extensions**
