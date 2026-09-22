<h1 align="center"><img src="docs/images/wordmark.svg" alt="Grisey, by Yulania" width="440"></h1>

Grisey is a free, open-source metering plugin for mixing and mastering. It shows level, loudness, true peak, spectrum, spectrogram, stereo image and phase correlation in one resizable window. Its loudness and true peak measurements follow ITU-R BS.1770-4 and EBU R 128, and are tested against synthesized versions of the EBU's test signals.

It is also meant to be read. The measurement code is kept apart from the interface and is covered by tests, and the [roadmap](ROADMAP.md) records why things are built the way they are, with the measurements behind each decision. If you have wondered how a loudness meter or a true peak detector works, the source may help. Questions, ideas and pull requests are welcome.

It runs as a VST3, AU or CLAP plugin, or as a standalone app, on macOS and Windows.

**Status:** version 2.0 is in beta. It has been used as a standalone app and in JUCE's AudioPluginHost on macOS, it passes Apple's `auval`, and every build is checked with [pluginval](https://github.com/Tracktion/pluginval) on macOS and Windows. It has not been tried in many hosts, and hardly at all by hand on Windows, so reports are welcome in the [issues](https://github.com/ZhiyuAlexZhang/Grisey/issues).

https://github.com/user-attachments/assets/0179e2fa-0496-4e1f-8aec-9bc1714591a3

<p align="center"><img src="docs/images/spectrum.png" alt="The spectrum view"></p>

| Spectrogram | Loudness |
| :---: | :---: |
| ![The spectrogram view](docs/images/spectrogram.png) | ![The loudness view](docs/images/loudness.png) |
| **History** | **Goniometer** |
| ![The history view](docs/images/history.png) | ![The goniometer view](docs/images/goniometer.png) |

The spectrogram, the loudness and the history above are pictures of the same half minute of a song, its intro and the moment the full mix comes in, because the three views share one timeline.

## Features

### Always in view
- Level bars for each channel: the RMS level, the peak level above it, and ticks that hold the highest peak. Every level is measured on the audio thread from every sample, so no peak is missed between frames.
- Loudness bars for the momentary and short-term loudness, with the integrated loudness and the target marked on them. They turn yellow at the target and red 6 LU above it.
- The integrated and short-term loudness, the loudness range, and the true peak as numbers.
- The correlation of the channels, from +1 (in phase) through 0 (wide stereo) to -1 (out of phase), as a fast and a slow reading.

### Spectrum
- FFT sizes from 2048 to 16384 points, scaled so that a full-scale sine reads 0 dB.
- Left / right or mid / side, adjustable tilt, fractional-octave smoothing, peak hold, and freeze.
- A correlation strip along the bottom shows, band by band, where the channels are in phase and where they are out of phase.
- The curves rise at once and fall back smoothly, with faint trails that show how they are moving.
- Under the mouse: the frequency, the nearest note, and the level.

### Spectrogram
- The top 66 dB of the spectrum over time, on a logarithmic frequency axis, with the same tilt and FFT size as the spectrum.
- Under the mouse: the frequency and the nearest note.

### Loudness
- Momentary, short-term, and integrated loudness, and loudness range, to ITU-R BS.1770-4 and EBU R 128.
- True peak with 4x oversampling, the peak to loudness ratio (PLR), and the peak to short-term loudness ratio (PSR).
- A graph of the momentary and short-term loudness from 0 to -48 LUFS, and delivery targets for streaming, podcasts, EBU R 128, and ATSC A/85.
- Tested against synthesized versions of the signals of EBU Tech 3341 and Tech 3342. The EBU's own recordings have not been run through it yet.

### History
- The peak and RMS levels over time, either or both.

### Goniometer
- The stereo image with phosphor-style persistence, as a Lissajous or a polar plot, with a knob for its scale.

### One timeline
- The spectrogram, the loudness graph and the history share one timeline. They all keep recording whichever view is showing, and they all show the same span of time (15, 30 or 60 s), so the same moment is in the same place when you switch between them.
- Resizing the window or changing the span keeps what has been recorded.
- One Reset button, in the bar with every view, starts every measurement again: the timeline, the integrated loudness and the range, the highest true peak, the spectrum's peak hold, and the peak ticks. Nothing is cleared by a click on a graph.

### Interface, formats and settings
- A resizable interface, from 860 x 480 to 2600 x 1600, in the manner of FabFilter's plugins. It redraws at 60 frames per second, or at 30, which takes less of the CPU.
- VST3, AU, CLAP, and Standalone. Mono and stereo.
- Every setting is saved with the session, and sessions saved by version 1, when it was called MultiMeter, still load.

## Installing

Grisey needs macOS 10.15 or later (Apple Silicon or Intel), or 64-bit Windows 10 or later.

Download the zip for your system from the [releases page](https://github.com/ZhiyuAlexZhang/Grisey/releases), and copy what you need:

| | macOS | Windows |
| --- | --- | --- |
| VST3 | `~/Library/Audio/Plug-Ins/VST3` | `C:\Program Files\Common Files\VST3` |
| AU | `~/Library/Audio/Plug-Ins/Components` | |
| CLAP | `~/Library/Audio/Plug-Ins/CLAP` | `C:\Program Files\Common Files\CLAP` |

The builds are not signed with a developer certificate or notarized, so macOS will refuse to load
them until you clear the quarantine flag that it puts on downloads. After copying, run this for each plugin you installed:

```bash
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Grisey.vst3
```

On Windows, SmartScreen may warn about the standalone app; choose "More info", then "Run anyway".
If you would rather not run unsigned builds, build from source as described below.

## Building

Grisey builds with CMake 3.22 or later and a C++20 compiler. JUCE 9 and
clap-juce-extensions are included as git submodules.

```bash
git clone --recurse-submodules https://github.com/ZhiyuAlexZhang/Grisey.git
cd Grisey
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

The VST3, AU (macOS only), CLAP, and Standalone builds are written to
`build/Grisey_artefacts/Release`. Add `-DGRISEY_COPY_AFTER_BUILD=ON` to install them
into your user plugin folders as part of the build.

To run the unit tests:

```bash
ctest --test-dir build -C Release --output-on-failure
```

## Developing

[ROADMAP.md](ROADMAP.md) records what version 2.0 changed and why, with the measurements behind the decisions, and what is still to do.

The measurement code in `Source/Engine` does not depend on the interface, and is covered by the unit tests, which include the synthesized test signals of EBU Tech 3341 and Tech 3342.

To save a picture of the editor running a test signal, without opening a host:

```bash
build/GriseySnapshot_artefacts/Release/GriseySnapshot editor.png 4
```

The number picks the view (0 goniometer, 1 spectrum, 2 spectrogram, 3 history, 4 loudness).
Parameters can be set by their IDs, for example `spectrumChannels=1`, and `size=1400x800` sets the
size of the editor. `also=2,3` stops the audio and then saves those views as well, all of the same
moment, which shows what each view recorded while it was hidden. `audio=song.mp3` plays a file
through the plugin in place of the test signal, and `from=30` starts it 30 s in, which is how the
pictures on this page were made. `frames=30` makes a film instead, at that rate and at twice the
size of the editor, through ffmpeg, and `show=2@8,4@15` changes to those views at those seconds, and `lead=7` starts the film 7 s in, so
the views of the timeline have something recorded when it begins. `click=Reset@20` presses the button of that name 20 s in. The
tool's window ignores the mouse, so that it cannot take a click that was meant for something else.

The same tool writes the name in the header as an outline (`GriseySnapshot out.svg wordmark
"Snell Roundhand" Bold Source/UI/Wordmark.h`), and draws a sample menu (`GriseySnapshot menu.png menu`),
because a menu closes as soon as its application is not in front.

## The name, and version 1

Grisey is named after Gérard Grisey, the composer who made the spectrum of a sound the material of his music.

Up to version 1 it was called MultiMeter. Hosts know a plugin by its codes rather than by its name, and Grisey has kept MultiMeter's, so a session that was saved with MultiMeter opens with Grisey in its place, with its settings. For the same reason the two cannot be installed side by side.

## Licence and credits

Grisey is free software under the GPLv3 (see [LICENSE.md](LICENSE.md)). It is built with
[JUCE](https://juce.com) 9, used under the AGPLv3, and
[clap-juce-extensions](https://github.com/free-audio/clap-juce-extensions).

The name in the header is set in Snell Roundhand, and drawn as an outline (`Source/UI/Wordmark.h`,
`docs/images/wordmark.svg`), so that it looks the same on a computer that does not have the typeface.
Yulania, to its left, is the maker's name.

The look of the interface is inspired by FabFilter's plugins. Grisey is an independent project:
it is not made, endorsed, or supported by FabFilter, and it uses none of their artwork or code.
