cmake_minimum_required(VERSION 3.22)

project(MatchboxEQ VERSION 1.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

option(MATCHBOX_BUILD_TESTS "Build the framework-free DSP regression tests" ON)

set(_matchbox_plugin_default OFF)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    set(_matchbox_plugin_default ON)
endif()
option(MATCHBOX_BUILD_PLUGIN "Build the JUCE VST3/AU/Standalone plug-in" ${_matchbox_plugin_default})

add_library(matchboxeq_dsp INTERFACE)
target_include_directories(matchboxeq_dsp INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
)
target_compile_features(matchboxeq_dsp INTERFACE cxx_std_17)

if(MATCHBOX_BUILD_TESTS)
    add_executable(matchboxeq_dsp_tests
        tests/test_matchbox_eq.cpp
    )
    target_link_libraries(matchboxeq_dsp_tests PRIVATE matchboxeq_dsp)
    if(MSVC)
        target_compile_options(matchboxeq_dsp_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(matchboxeq_dsp_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME matchboxeq_dsp_tests COMMAND matchboxeq_dsp_tests)
endif()

if(MATCHBOX_BUILD_PLUGIN)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
        message(FATAL_ERROR
            "MATCHBOX_BUILD_PLUGIN=ON, but JUCE/CMakeLists.txt is missing. "
            "Place a pinned JUCE checkout in JUCE/ or configure with "
            "-DMATCHBOX_BUILD_PLUGIN=OFF to run the DSP tests."
        )
    endif()

    add_subdirectory(JUCE)

    set(matchbox_plugin_formats VST3 Standalone)
    if(APPLE)
        list(APPEND matchbox_plugin_formats AU)
    endif()

    juce_add_plugin(MatchboxEQ
        COMPANY_NAME "FAUSTpianist"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        PLUGIN_MANUFACTURER_CODE Fpst
        PLUGIN_CODE Mbeq
        FORMATS ${matchbox_plugin_formats}
        PRODUCT_NAME "MatchboxEQ"
    )

    target_sources(MatchboxEQ PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
    )

    target_link_libraries(MatchboxEQ
        PRIVATE
            matchboxeq_dsp
            juce::juce_audio_utils
            juce::juce_dsp
            juce::juce_gui_basics
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags
    )

    target_compile_definitions(MatchboxEQ PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
    )
endif()
