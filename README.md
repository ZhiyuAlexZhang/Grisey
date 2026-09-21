# MultiMeter

## Overview

MultiMeter, a cutting-edge AU/VST3/CLAP audio analyzer, caters to audio engineers, producers, and musicians who seek precision and versatility. Leveraging the JUCE framework, MultiMeter delivers a robust array of features for pristine real-time audio analysis, enhancing mixing, mastering, and sound design processes.

![multimeter-demo](https://github.com/RealAlexZ/MultiMeter/assets/97690118/ce64ecb6-801e-4e9d-8815-1f97655c272d)

## Features

### Always in view
- Level bars for each channel: the RMS level, the peak level above it, and ticks that hold the highest peak. Every level is measured on the audio thread from every sample, so no peak is missed between frames.
- Loudness bars for the momentary and short-term loudness, with the integrated loudness and the target marked on them.
- The integrated and short-term loudness, the loudness range, and the true peak as numbers.
- The correlation of the channels, from +1 (in phase) through 0 (wide stereo) to -1 (out of phase), as a fast and a slow reading.

### Loudness
- Momentary, short-term, and integrated loudness, and loudness range, to ITU-R BS.1770-4 and EBU R 128.
- True peak with 4x oversampling, the peak to loudness ratio (PLR), and the peak to short-term loudness ratio (PSR).
- A history of the last minute, and delivery targets for streaming, podcasts, EBU R 128, and ATSC A/85.
- Tested against the synthesized signals of EBU Tech 3341 and Tech 3342.

### Spectrum
- FFT sizes from 2048 to 16384 points, scaled so that a full-scale sine reads 0 dB.
- Left / right or mid / side, adjustable tilt, fractional-octave smoothing, peak hold, and freeze.
- A correlation strip along the bottom shows, band by band, where the channels are in phase and where they are out of phase.
- Under the mouse: the frequency, the nearest note, and the level.

### Spectrogram
- Frequency content over time, on a logarithmic frequency axis.

### Goniometer
- The stereo image with phosphor-style persistence, as a Lissajous or a polar plot.

### History
- The peak and RMS levels over the last half minute.

### Interface, formats and settings
- A resizable interface in the style of FabFilter's plugins, which redraws at 30 or 60 frames per second.
- VST3, AU, CLAP, and Standalone. Mono and stereo.
- Every setting is saved with the session, and sessions saved by version 1 still load.

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
build/MultiMeterSnapshot_artefacts/Release/MultiMeterSnapshot editor.png 4
```

The number picks the view (0 goniometer, 1 spectrum, 2 spectrogram, 3 history, 4 loudness).
Parameters can be set by their IDs, for example `spectrumChannels=1`, and `size=1400x800` sets the
size of the editor. The tool's window ignores the mouse, so that it cannot take a click that was
meant for something else.

## Roadmap

Version 2.0 is in progress. See [ROADMAP.md](ROADMAP.md) for the plan.

## Dependencies
- **JUCE:** 9.0.2
- **clap-juce-extensions**
