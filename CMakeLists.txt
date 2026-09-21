cmake_minimum_required(VERSION 3.22)

project(Grisey VERSION 2.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(GRISEY_COPY_AFTER_BUILD "Install the built plugins into the user plugin folders" OFF)

add_subdirectory(JUCE)

# Grisey was called MultiMeter up to version 1. The manufacturer and plugin codes are the
# ones that MultiMeter had, because they are what a host knows a plugin by: sessions saved
# with MultiMeter open with Grisey in its place. Do not install both at once.
juce_add_plugin(Grisey
    COMPANY_NAME "Yulania"
    BUNDLE_ID "com.Yulania.Grisey"
    PLUGIN_MANUFACTURER_CODE Manu
    PLUGIN_CODE Epc7
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Grisey"
    DESCRIPTION "Grisey"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Fx Analyzer
    AU_MAIN_TYPE kAudioUnitType_Effect
    AU_EXPORT_PREFIX GriseyAU
    COPY_PLUGIN_AFTER_BUILD ${GRISEY_COPY_AFTER_BUILD})

juce_generate_juce_header(Grisey)

target_sources(Grisey PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Views/GoniometerView.cpp
    Source/Views/HistoryView.cpp
    Source/Views/LevelMeters.cpp
    Source/Views/LoudnessSummary.cpp
    Source/Views/LoudnessView.cpp
    Source/Views/SpectrogramView.cpp
    Source/Views/SpectrumView.cpp)

target_compile_definitions(Grisey PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Grisey
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

#==============================================================================
# JUCE has no native CLAP target, so the CLAP build comes from clap-juce-extensions.
option(GRISEY_BUILD_CLAP "Build the CLAP plugin" ON)

if(GRISEY_BUILD_CLAP)
    add_subdirectory(libs/clap-juce-extensions EXCLUDE_FROM_ALL)

    clap_juce_extensions_plugin(TARGET Grisey
        CLAP_ID "com.Yulania.Grisey"
        CLAP_FEATURES audio-effect analyzer utility stereo mono)
endif()

#==============================================================================
option(GRISEY_BUILD_TESTS "Build the unit tests" ON)

if(GRISEY_BUILD_TESTS)
    enable_testing()

    # Adds a console tool that uses the plugin's code. It links against the plugin's
    # shared-code library, which already contains the JUCE modules, so it only borrows
    # that library's include paths and definitions rather than compiling the modules again.
    function(grisey_add_tool name)
        juce_add_console_app(${name} PRODUCT_NAME "${name}")
        target_sources(${name} PRIVATE ${ARGN})
        target_include_directories(${name} PRIVATE
            $<TARGET_PROPERTY:Grisey,INCLUDE_DIRECTORIES>)
        target_compile_definitions(${name} PRIVATE
            $<TARGET_PROPERTY:Grisey,COMPILE_DEFINITIONS>)
        target_link_libraries(${name} PRIVATE Grisey)
    endfunction()

    grisey_add_tool(GriseyTests
        Tests/Main.cpp
        Tests/ProcessorTests.cpp
        Tests/LoudnessTests.cpp
        Tests/SpectrumTests.cpp)

    add_test(NAME GriseyTests COMMAND GriseyTests)

    # A development tool that saves a picture of the editor running a test signal
    grisey_add_tool(GriseySnapshot Tests/Snapshot.cpp)
endif()
