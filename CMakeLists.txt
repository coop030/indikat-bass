cmake_minimum_required(VERSION 3.22)

project(IndikatBass VERSION 1.2.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: entweder als Unterordner "JUCE" (git clone) oder automatisch per FetchContent laden
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_binary_data(IndikatFonts SOURCES
    Resources/Michroma-Regular.ttf
    Resources/Questrial-Regular.ttf
    Resources/ShareTechMono-Regular.ttf
    Resources/indikat-logo.svg)

set(DUBBASS_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/BassSynth.cpp
    Source/BlackHole.cpp)

# Effekt für vorhandene Basslines
juce_add_plugin(IndikatBassFX
    COMPANY_NAME "INDIKAT"
    BUNDLE_ID com.indikat.bassfx
    PLUGIN_MANUFACTURER_CODE Indk
    PLUGIN_CODE Ibfx
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "INDIKAT Bass FX"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Filter Delay)

# Bass-Synth mit 2 Oszillatoren + derselben Effektkette
juce_add_plugin(IndikatBassSynth
    COMPANY_NAME "INDIKAT"
    BUNDLE_ID com.indikat.basssynth
    PLUGIN_MANUFACTURER_CODE Indk
    PLUGIN_CODE Ibsy
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "INDIKAT Bass Synth"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    VST3_CATEGORIES Instrument Synth)

foreach(target IndikatBassFX IndikatBassSynth)
    target_sources(${target} PRIVATE ${DUBBASS_SOURCES})

    target_compile_definitions(${target} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

    target_link_libraries(${target}
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
            IndikatFonts
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endforeach()

target_compile_definitions(IndikatBassSynth PRIVATE DUBBASS_SYNTH=1)

# Offline-Test: rendert alle Synth-Presets mit einer Bassline als WAV (cmake -DINDIKAT_TESTS=ON)
option(INDIKAT_TESTS "Build offline render test" OFF)
if(INDIKAT_TESTS)
    juce_add_console_app(IndikatRenderTest PRODUCT_NAME "IndikatRenderTest")
    target_sources(IndikatRenderTest PRIVATE Tests/RenderTest.cpp ${DUBBASS_SOURCES})
    target_compile_definitions(IndikatRenderTest PRIVATE
        DUBBASS_SYNTH=1 JucePlugin_Name="INDIKAT Bass Synth" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(IndikatRenderTest PRIVATE juce::juce_audio_utils juce::juce_dsp IndikatFonts
        juce::juce_recommended_config_flags)
endif()
