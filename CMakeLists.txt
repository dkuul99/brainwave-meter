cmake_minimum_required(VERSION 3.22)
project(BrainwaveMeter VERSION 1.0.0)

set(JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE" CACHE PATH "Path to JUCE")

add_subdirectory(${JUCE_PATH} JUCE)

juce_add_plugin(BrainwaveMeter
    COMPANY_NAME           "OWL"
    PLUGIN_MANUFACTURER_CODE Owl0
    PLUGIN_CODE             Bwm0
    PLUGIN_NAME            "Brainwave Meter"
    PLUGIN_DESCRIPTION     "Real-time brainwave entrainment analyzer"
    FORMATS                VST3 Standalone
    PRODUCT_NAME           "Brainwave Meter"
    IS_SYNTH               FALSE
    NEEDS_MIDI_INPUT       FALSE
    NEEDS_MIDI_OUTPUT      FALSE
    IS_MIDI_EFFECT         FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
)

target_sources(BrainwaveMeter
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

target_compile_definitions(BrainwaveMeter
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3=1
        JUCE_VST2=0
        JUCE_STANDALONE=1
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(BrainwaveMeter
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
