cmake_minimum_required (VERSION 3.22)
project (MatchboxEQ VERSION 1.0.0)

set (CMAKE_CXX_STANDARD 17)

# Universal Binary on macOS (Apple Silicon + Intel)
if (APPLE)
    set (CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build Universal Binary")
endif()

# Point this at your JUCE install (clone https://github.com/juce-framework/JUCE)
add_subdirectory (JUCE)

juce_add_plugin (MatchboxEQ
    COMPANY_NAME "FAUSTpianist"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Fpst
    PLUGIN_CODE Mbeq
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "MatchboxEQ")

target_sources (MatchboxEQ PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions (MatchboxEQ PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries (MatchboxEQ
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
