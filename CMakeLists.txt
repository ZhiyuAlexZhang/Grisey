cmake_minimum_required(VERSION 3.22)

project(MultiMeter VERSION 2.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(MULTIMETER_COPY_AFTER_BUILD "Install the built plugins into the user plugin folders" OFF)

add_subdirectory(JUCE)

# The manufacturer and plugin codes match the original Projucer project, so
# sessions saved with v1 keep loading.
juce_add_plugin(MultiMeter
    COMPANY_NAME "Yulania"
    BUNDLE_ID "com.Yulania.MultiMeter"
    PLUGIN_MANUFACTURER_CODE Manu
    PLUGIN_CODE Epc7
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "MultiMeter"
    DESCRIPTION "MultiMeter"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Fx Analyzer
    AU_MAIN_TYPE kAudioUnitType_Effect
    AU_EXPORT_PREFIX MultiMeterAU
    COPY_PLUGIN_AFTER_BUILD ${MULTIMETER_COPY_AFTER_BUILD})

juce_generate_juce_header(MultiMeter)

target_sources(MultiMeter PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Controls/Slider.cpp
    Source/CorrelationMeter/CorrelationMeter.cpp
    Source/GonioMeter/Goniometer.cpp
    Source/Histogram/Histogram.cpp
    Source/LevelMeter/LevelMeter.cpp
    Source/LoudnessView/LoudnessView.cpp
    Source/Spectrogram/Spectrogram.cpp
    Source/SpectrumAnalyzer/SpectrumAnalyzer.cpp)

target_compile_definitions(MultiMeter PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(MultiMeter
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

#==============================================================================
# JUCE has no native CLAP target, so the CLAP build comes from clap-juce-extensions.
option(MULTIMETER_BUILD_CLAP "Build the CLAP plugin" ON)

if(MULTIMETER_BUILD_CLAP)
    add_subdirectory(libs/clap-juce-extensions EXCLUDE_FROM_ALL)

    clap_juce_extensions_plugin(TARGET MultiMeter
        CLAP_ID "com.Yulania.MultiMeter"
        CLAP_FEATURES audio-effect analyzer utility stereo mono)
endif()

#==============================================================================
option(MULTIMETER_BUILD_TESTS "Build the unit tests" ON)

if(MULTIMETER_BUILD_TESTS)
    enable_testing()

    # Adds a console tool that uses the plugin's code. It links against the plugin's
    # shared-code library, which already contains the JUCE modules, so it only borrows
    # that library's include paths and definitions rather than compiling the modules again.
    function(multimeter_add_tool name)
        juce_add_console_app(${name} PRODUCT_NAME "${name}")
        target_sources(${name} PRIVATE ${ARGN})
        target_include_directories(${name} PRIVATE
            $<TARGET_PROPERTY:MultiMeter,INCLUDE_DIRECTORIES>)
        target_compile_definitions(${name} PRIVATE
            $<TARGET_PROPERTY:MultiMeter,COMPILE_DEFINITIONS>)
        target_link_libraries(${name} PRIVATE MultiMeter)
    endfunction()

    multimeter_add_tool(MultiMeterTests
        Tests/Main.cpp
        Tests/ProcessorTests.cpp
        Tests/LoudnessTests.cpp
        Tests/SpectrumTests.cpp)

    add_test(NAME MultiMeterTests COMMAND MultiMeterTests)

    # A development tool that saves a picture of the editor running a test signal
    multimeter_add_tool(MultiMeterSnapshot Tests/Snapshot.cpp)
endif()
