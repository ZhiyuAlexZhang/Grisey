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

## Phase 1: Engine rewrite

- [ ] Compute peak, RMS, and correlation on the audio thread from every sample, and publish
      them to the GUI through atomics or a lock-free snapshot. Today the editor measures only
      the last block it pulls per frame, so peaks in the other blocks are missed.
- [ ] Replace the block FIFOs with one sample ring buffer that feeds the scope and the FFT
- [ ] Move every setting into APVTS and save versioned state. Today settings are written as a
      raw binary stream outside the parameter tree.
- [ ] Replace the five separate 60 Hz timers with one `VBlankAttachment`
- [ ] Unit tests for each measurement

## Phase 2: New meters

- [ ] Loudness to ITU-R BS.1770-4 / EBU R128: momentary, short-term, integrated, loudness
      range, a history graph, and delivery targets (for example -14 LUFS for streaming)
- [ ] True peak with 4x oversampling, plus PLR and PSR
- [ ] Spectrum: overlapped FFT, adjustable tilt, fractional-octave smoothing, peak hold,
      freeze, and L/R and M/S modes
- [ ] A scrolling spectrogram
- [ ] Goniometer with phosphor-style persistence, and Lissajous and polar modes
- [ ] Multiband correlation
- [ ] Verify the loudness meter against the EBU Tech 3341 / 3342 test signals

## Phase 3: Interface

- [ ] Dark theme driven by one theme struct and one `LookAndFeel`, replacing the colour macros
- [ ] One large resizable, HiDPI display with floating controls that fade in on hover
- [ ] Gradient-filled curves, smooth meter ballistics, and hover readouts (frequency, note, dB)
- [ ] Cache static layers as images, and profile before adding any GPU shader

## Phase 4: Release

- [ ] Signed and notarized macOS installer, and a Windows installer
- [ ] New README, screenshots, and demo
- [ ] Tag v2.0.0
