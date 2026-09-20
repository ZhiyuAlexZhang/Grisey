# MultiMeter 2.0 Roadmap

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
- [x] `MultiMeterSnapshot`, a tool that saves a picture of the editor running a test signal

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

## Phase 3: Interface

- [ ] Dark theme driven by one theme struct and one `LookAndFeel`, replacing the colour macros
- [ ] One large resizable, HiDPI display with floating controls that fade in on hover
- [ ] Gradient-filled curves, smooth meter ballistics, and hover readouts (frequency, note, dB)
- [ ] Cut the cost of drawing, which is nearly all of the plugin's CPU use: about 25-30% of
      one core with an active signal, in every view, against about 3% for our own code.
      Cache static layers as images, repaint only what changes, and profile before adding
      any GPU shader.
- [ ] Give the view options proper controls. For now they are a plain row of combo boxes.

## Phase 4: Release

- [ ] Signed and notarized macOS installer, and a Windows installer
- [ ] New README, screenshots, and demo
- [ ] Tag v2.0.0
