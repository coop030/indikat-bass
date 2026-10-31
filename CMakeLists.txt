cmake_minimum_required(VERSION 3.22)
project(IndikatStarPad VERSION 1.0.1)

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

juce_add_binary_data(StarPadData SOURCES
    Resources/Michroma-Regular.ttf
    Resources/Questrial-Regular.ttf
    Resources/ShareTechMono-Regular.ttf
    Resources/indikat-logo.svg)

# Beim Cross-Compilen (Windows-Build unter Linux) kann das VST3-Manifest-Tool nicht laufen
if(CMAKE_CROSSCOMPILING)
    set(STARPAD_VST3_MANIFEST FALSE)
else()
    set(STARPAD_VST3_MANIFEST TRUE)
endif()

juce_add_plugin(IndikatStarPad
    COMPANY_NAME "INDIKAT"
    PRODUCT_NAME "INDIKAT Star Pad"
    PLUGIN_MANUFACTURER_CODE Idkt
    PLUGIN_CODE Stpd
    FORMATS VST3 AU Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    VST3_AUTO_MANIFEST ${STARPAD_VST3_MANIFEST})

target_sources(IndikatStarPad PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/StarPadEngine.cpp)

target_compile_definitions(IndikatStarPad PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(IndikatStarPad
    PRIVATE
        StarPadData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline-Test: rendert Demo-WAVs typischer Szenen und prüft Pegel (cmake -DSTARPAD_TESTS=ON)
option(STARPAD_TESTS "Build offline render test" OFF)
if(STARPAD_TESTS)
    juce_add_console_app(StarPadRender PRODUCT_NAME "StarPadRender")
    target_sources(StarPadRender PRIVATE Tests/Render.cpp Source/StarPadEngine.cpp)
    target_compile_definitions(StarPadRender PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(StarPadRender PRIVATE juce::juce_audio_formats juce::juce_dsp
                          PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
if(STARPAD_TESTS)
    juce_add_console_app(StarPadStress PRODUCT_NAME "StarPadStress")
    target_sources(StarPadStress PRIVATE Tests/Stress.cpp Source/StarPadEngine.cpp)
    target_compile_definitions(StarPadStress PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(StarPadStress PRIVATE juce::juce_audio_formats juce::juce_dsp
                          PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
if(STARPAD_TESTS)
    juce_add_console_app(StarPadPerf PRODUCT_NAME "StarPadPerf")
    target_sources(StarPadPerf PRIVATE Tests/Perf.cpp Source/StarPadEngine.cpp)
    target_compile_definitions(StarPadPerf PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(StarPadPerf PRIVATE juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)
endif()
if(STARPAD_TESTS)
    juce_add_console_app(StarPadPresets PRODUCT_NAME "StarPadPresets")
    target_sources(StarPadPresets PRIVATE Tests/Presets.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/StarPadEngine.cpp)
    target_compile_definitions(StarPadPresets PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="INDIKAT Star Pad")
    target_link_libraries(StarPadPresets PRIVATE StarPadData juce::juce_audio_utils juce::juce_dsp
                          PUBLIC juce::juce_recommended_config_flags)
endif()
if(STARPAD_TESTS)
    juce_add_gui_app(StarPadScreenshot PRODUCT_NAME "StarPadScreenshot")
    target_sources(StarPadScreenshot PRIVATE Tests/Screenshot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/StarPadEngine.cpp)
    target_compile_definitions(StarPadScreenshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1 JucePlugin_Name="INDIKAT Star Pad")
    target_link_libraries(StarPadScreenshot PRIVATE StarPadData juce::juce_audio_utils juce::juce_dsp
                          PUBLIC juce::juce_recommended_config_flags)
endif()
