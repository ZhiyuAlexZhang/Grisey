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
      ones in S-shaped shoulders, a display that runs from black to a dark, faintly purple grey, a
      thin grey grid, a yellow curve for the main signal and a blue one for the second, knobs with
      a blue ring, a dotted scale and a white lens for a pointer, floating panels
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
      - The frame rate is what decides the cost, so it is a setting: 60 frames per second by
        default (14-26% of a core per view), which is what a meter ought to look like, or 30
        (10-15%). The version 1 style interface took 20-39% at 60 in a smaller window. An idle
        editor takes about 4%.
- [x] One timeline for the spectrogram, the history and the loudness (`Source/Views/Timeline.h`).
      The spectrogram used to record only while it was the view that was showing, so it fell
      behind the others and had a hole for every time it was hidden, and the three views each
      showed a different stretch of time (one of them depending on the frame rate and the width of
      the window). Now one clock counts slots of a thirtieth of a second, every view records every
      slot whether it is showing or not, and all three show the same span, which is a setting (15,
      30 or 60 s). What is recorded is kept as data, so resizing the window or changing the span
      draws the pictures again without losing anything. Freezing holds the pictures still while
      the recording carries on underneath.
- [x] The spectrum moves like an analyzer should: the curves rise to a new level in 25 ms and fall
      back over 300 ms, in every frame rather than only when there is a new spectrum, with three
      faint trails of where they were, a glow beneath the line that fades out downwards, and a
      smooth curve through the few bins at low frequencies, where straight lines had shown as a
      row of arches. The curve through the bins is a monotone spline: a Catmull-Rom spline was
      tried first and read a full-scale tone 6.8 dB high, because a tone stands 100 dB above the
      bins beside it. The trails are drawn at a third of the resolution, which brought the view
      back to what it cost before (14% of a core at 30 frames per second).
- [x] The colors, after measuring those of a picture of Pro-R 2 rather than judging them by eye:
      - A curve carries its color in its line, with a wide, faint stroke beneath it for a bloom.
        What is under it is only its light, brightest under the top of the curve and gone a little
        more than half way down the plot. Whole areas had been filled flat, and large areas of
        flat color look heavy: the history was a slab of blue and the loudness graph a brown block.
      - The olive of the spectrum was not the yellow, whose hue is the same as FabFilter's amber.
        It was the blue light of the second curve under the yellow light of the first, so only the
        first curve has a light beneath it.
      - FabFilter's blue is a steel blue (#22648d to #6289a2), where ours was a sky blue. One
        function in `Theme.h` makes the scale of colors for the level bars, the history and the
        loudness bars, which turn yellow at the target and red 6 LU above it.
      - The spectrogram showed 90 dB from a bright blue to a bright yellow, which lit even what
        was quiet and made a grey green of everything in between. It shows 66 dB, keeps what is
        quiet dark with a power of 1.5, and goes from black through a hint of the meters' blue to
        amber, yellow and a warm white. The blue meets the amber while both are dark. It records
        512 rows, and the scrolling images are drawn with smoothing.
- [x] One Reset button, beside Meters in the bar with every view, starts every measurement again:
      the loudness and the true peak, the timeline in all three views, the spectrum's peak hold,
      and the ticks. Nothing is cleared by a click on a graph, where a click meant for something
      else could throw a minute away. This is what the manuals of Pro-L 2, Insight 2 and Youlean
      Loudness Meter describe. A click on the level bars still resets their ticks, as a click on
      a peak reading does everywhere. What the history shows is a setting in its own bar.
- [x] Each loudness reading is in one place. The loudness view had a panel of numbers of which
      four were also in the side column, and the two took their readings on clocks of their own,
      so they could differ by a tenth. The editor now decides when a readout is due for every
      number at once, and the view shows only what is its own (momentary, PLR, PSR) above a graph
      that has the full width and reaches from 0 to -48 LUFS whatever the target is.
- [x] The menus have the look of the interface. A popup menu takes its look and feel only from
      what it is given itself, not from the component that opens it, so every menu had come up in
      JUCE's own colors. They are drawn in the manner of FabFilter's: rows close together, a solid
      blue bar under the mouse, the current choice in yellow with a dot, and a space between groups.
- [x] The tabs are in order of use: the spectrum, which opens first, then the spectrogram beside
      it, then the three views of the timeline with the loudness first, and the goniometer last.
      The values of the parameter did not change, so saved sessions open on the view they had.
- [x] The name in the header is an outline of Snell Roundhand Bold (`Source/UI/Wordmark.h`, written
      by the snapshot tool), so that it is the same on a computer without the typeface, set into
      the surface with a flourish on either side. The maker's name is to its left on the same line,
      in a place that does not depend on the product's letters, so that other plugins of the
      maker's can have it in the same place. The tab starts at the edge of the window and is as
      wide as the line in it.
- [x] The goniometer's trace is exposed as brightly as the spectrum's line: one pass of the beam is a
      bright yellow, and where passes pile up the trace goes on to white. It had been a fifth as
      bright, a transparent ochre. The snapshot tool makes films (`frames=`, `show=`, `lead=`), at
      the size of a Retina display, through ffmpeg.
- [ ] Try everything that a picture cannot show, by hand: the menus, the tabs, the readouts under
      the mouse, dragging the knob and the corner of the window, and the reset buttons
- [ ] The spectrogram and the history have one image column per slot of the timeline, stretched
      over the plot, so they are a little soft on a high-resolution display
- [ ] From the manuals of other meters: resetting when the host starts playback, one pause for all
      three views of the timeline, and readouts under the mouse on the history and the loudness

## Phase 4: Release

- [ ] Signed and notarized macOS installer, and a Windows installer
- [x] New README and screenshots, taken with music by the snapshot tool
- [x] A release workflow that builds both systems, signs the macOS bundles ad hoc as a whole, and
      makes a draft pre-release (`v2.0.0-beta.1`)
- [ ] A demo
- [ ] Tag v2.0.0
