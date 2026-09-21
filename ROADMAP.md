# Grisey 2.0 Roadmap

Grisey was called MultiMeter up to version 1. It is named after Gérard Grisey, the composer who
made the spectrum of a sound the material of his music.

The goal of 2.0 is a metering plugin that is accurate first and good-looking second: every
reading is computed on the audio thread from every sample, and the interface is a dark,
resizable, GPU-friendly design in the spirit of modern mastering tools.

## Phase 0: Foundation (done)

- [x] Move the build from Projucer to CMake, with JUCE 9 as a submodule
- [x] Build VST3, AU, Standalone, and CLAP (via clap-juce-extensions)
- [x] Keep the v1 plugin and manufacturer codes so existing sessions still load
- [x] Fix the main FIFO, whose read and write calls were swapped (the cause of the
      "meters not responding" workaround in the old README)
- [x] Fix the crash on mono layouts, where channel 1 was read unconditionally
- [x] Stop `Fifo::push` from allocating on the audio thread
- [x] Fix the goniometer reading past the end of blocks shorter than 256 samples
- [x] Correct the `Channel` enum, which had left and right reversed
- [x] Add unit tests and a GitHub Actions build (macOS, Windows) that runs pluginval

## Phase 1: Engine rewrite (done)

- [x] Measure peak, RMS, and correlation on the audio thread from every sample
      (`Source/Engine/MeterEngine.h`), and publish them to the GUI through atomics. Version 1
      measured only the last block of each frame, so it missed peaks in the other blocks.
- [x] Replace the block FIFOs with one lock-free sample ring buffer
      (`Source/Engine/SampleRingBuffer.h`) that feeds the goniometer and the FFT
- [x] Move every setting into APVTS (`Source/Parameters.h`) and save versioned XML state.
      Sessions saved by version 1 still load their settings.
- [x] Replace the fourteen 60 Hz timers with one `VBlankAttachment`. Meter ballistics are
      now time-based, so they behave the same on 60 Hz and 120 Hz displays.
- [x] Show silence when the host stops sending audio, instead of freezing the last reading
- [x] Make "inf" tick hold truly infinite (it was 60 seconds)
- [x] Unit tests for each measurement, the ring buffer, and state loading
- [x] `GriseySnapshot`, a tool that saves a picture of the editor running a test signal

## Phase 2: New meters (done)

- [x] Loudness to ITU-R BS.1770-4 / EBU R 128 (`Source/Engine/LoudnessMeter.h`): momentary,
      short-term, gated integrated, and loudness range, with a history graph and delivery
      targets (-14 LUFS streaming, -16 podcast, -23 EBU R 128, -24 ATSC A/85)
- [x] True peak with 4x oversampling (`Source/Engine/TruePeakDetector.h`), plus PLR and PSR.
      The steady-state error is within 0.17 dB up to 20 kHz.
- [x] Spectrum (`Source/Engine/SpectrumEngine.h`): overlapped Hann FFT from 2048 to 16384
      points, scaled so that a full-scale sine reads 0 dB, with tilt, fractional-octave
      smoothing, peak hold, freeze, and L/R and M/S modes. The curves now line up with the
      grid, which version 1 drew 12 dB apart.
- [x] A scrolling spectrogram
- [x] Goniometer with phosphor-style persistence, and Lissajous and polar modes
- [x] Multiband correlation, as a strip along the bottom of the analyzer that is blue where
      the channels are in phase and red where they are out of phase. It comes from the
      cross-spectrum of the same FFTs, so it is measured per analysis frame on the GUI
      thread, unlike the level, loudness, and wideband correlation readings, which are
      measured from every sample on the audio thread.
- [x] Verify against the EBU test signals: the tests synthesize Tech 3341 cases 1-5, 9, 12
      and 15-19, and Tech 3342 cases 1-4, and all pass
- [ ] Verify against the EBU cases that need the official recordings (Tech 3341 cases 7, 8
      and 20-23, Tech 3342 cases 5 and 6), which have to be downloaded from tech.ebu.ch

The audio-thread measurement (peak, RMS, correlation, loudness, true peak, ring buffer)
costs about 0.15% of one core at 48 kHz.

## Phase 3: Interface (done, apart from testing by hand)

- [x] A theme in one file (`Source/UI/Theme.h`) and one `LookAndFeel`, replacing the colour macros
      and all of the version 1 interface code
- [x] The look follows the design language of FabFilter's plugins, studied from a Figma recreation
      of Pro-R and a screenshot of Pro-MB: charcoal chrome whose raised parts meet the recessed
      ones in S-shaped shoulders, a display that runs from black to a deep purple, a thin grey
      grid, a yellow curve for the main signal and a blue one for the second with flat translucent
      fills, knobs with a blue ring, a dotted scale and a white lens for a pointer, floating panels
      with a light edge, and bars that read "Label: value". It copies no artwork, and uses the
      system's humanist typefaces (Avenir Next, Segoe UI) where FabFilter uses Frutiger.
- [x] One large resizable display (860x480 to 2600x1600, remembered with the session), a header of
      tabs, a bar of controls that changes with the view, and a side column that always shows the
      level and loudness bars, the loudness readings, and the correlation
- [x] Readouts under the mouse: frequency, note and level on the spectrum, frequency and note on
      the spectrogram
- [x] A floating scale knob on the goniometer, which brightens when the mouse is over the view
- [x] The two histograms became one scrolling history of the peak and RMS levels
- [x] Cut the cost of drawing. What was measured, on an M3 Max with a 1000x580 window:
      - A frame costs a flush of the whole window, whatever is drawn in it. Repainting one small
        bar at 60 frames per second costs as much as all the meters (about 15% of a core), and
        repainting the whole window only a little more (21%). So repainting less gains little.
      - JUCE's Metal layer renderer (`JUCE_COREGRAPHICS_RENDER_WITH_MULTIPLE_PAINT_CALLS`) and an
        attached `OpenGLContext` were both worse, and a `juce::Timer` was worse than vblank
        callbacks that skip frames.
      - The frame rate is what decides the cost, so it is a setting: 30 frames per second by
        default (10-15% of a core per view), or 60 (14-26%). The version 1 style interface took
        20-39% at 60 in a smaller window. An idle editor takes about 4%.
- [ ] Try everything that a picture cannot show, by hand: the menus, the tabs, the readouts under
      the mouse, dragging the knob and the corner of the window, and the reset buttons
- [ ] The spectrogram and the history are one image pixel per point, so they are a little soft on
      a high-resolution display

## Phase 4: Release

- [ ] Signed and notarized macOS installer, and a Windows installer
- [ ] New README, screenshots, and demo
- [ ] Tag v2.0.0
