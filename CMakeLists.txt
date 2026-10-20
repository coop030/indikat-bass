cmake_minimum_required(VERSION 3.22)
project(IndikatPluto VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: per -DJUCE_DIR=..., neben dem Projekt (../JUCE) oder automatisch per FetchContent
if(NOT DEFINED JUCE_DIR AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE/CMakeLists.txt")
    set(JUCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../JUCE)
endif()
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_binary_data(PlutoData SOURCES
    Resources/Michroma-Regular.ttf
    Resources/Questrial-Regular.ttf
    Resources/ShareTechMono-Regular.ttf
    Resources/indikat-logo.svg)

# Beim Cross-Compilen (Windows-Build unter Linux) kann das VST3-Manifest-Tool nicht laufen
if(CMAKE_CROSSCOMPILING)
    set(PLUTO_VST3_MANIFEST FALSE)
else()
    set(PLUTO_VST3_MANIFEST TRUE)
endif()

juce_add_plugin(IndikatPluto
    COMPANY_NAME "INDIKAT"
    PRODUCT_NAME "INDIKAT Pluto"
    PLUGIN_MANUFACTURER_CODE Idkt
    PLUGIN_CODE Plto
    FORMATS VST3 AU Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Generator
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_AUTO_MANIFEST ${PLUTO_VST3_MANIFEST})

target_sources(IndikatPluto PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PlutoEngine.cpp)

target_compile_definitions(IndikatPluto PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(IndikatPluto
    PRIVATE
        PlutoData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline-Test: rendert Demo-WAVs mehrerer Zufallsszenen (cmake -DPLUTO_TESTS=ON)
option(PLUTO_TESTS "Build offline render test" OFF)
if(PLUTO_TESTS)
    juce_add_console_app(PlutoRender PRODUCT_NAME "PlutoRender")
    target_sources(PlutoRender PRIVATE Tests/Render.cpp Source/PlutoEngine.cpp)
    target_compile_definitions(PlutoRender PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(PlutoRender PRIVATE juce::juce_audio_formats juce::juce_dsp
                          PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
