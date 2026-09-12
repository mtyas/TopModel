cmake_minimum_required(VERSION 3.22)
project(ModelKeys VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# 1. Fetch JUCE 8
message(STATUS "Fetching JUCE 8...")
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.6
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(juce)

# 2. Fetch CLAP JUCE Extensions
message(STATUS "Fetching clap-juce-extensions...")
FetchContent_Declare(
    clap-juce-extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(clap-juce-extensions)

# Sources
set(MODELKEYS_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Presets/PresetManager.cpp
    Source/UI/CustomLookAndFeel.cpp
    Source/UI/VisualizerComponent.cpp
)

juce_add_plugin(ModelKeys
    COMPANY_NAME "mtyas"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    PLUGIN_MANUFACTURER_CODE Mtya
    PLUGIN_CODE TpMd
    FORMATS VST3 Standalone
    PRODUCT_NAME "TopModel"
)

target_sources(ModelKeys PRIVATE ${MODELKEYS_SOURCES})

target_compile_definitions(ModelKeys PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(ModelKeys PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_extra
    juce_recommended_config_flags
    juce_recommended_warning_flags
)

# Add CLAP plugin target if available
if (COMMAND clap_juce_extensions_plugin)
    clap_juce_extensions_plugin(TARGET ModelKeys
        CLAP_ID "com.mtyas.topmodel"
        CLAP_FEATURES instrument synthesizer
    )
endif()

# Headless DSP Test Suite Executable
add_executable(ModelKeys_Tests
    Source/Tests/AudioTests.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Presets/PresetManager.cpp
    Source/UI/CustomLookAndFeel.cpp
    Source/UI/VisualizerComponent.cpp
)

target_compile_definitions(ModelKeys_Tests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=0
)

target_link_libraries(ModelKeys_Tests PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_gui_extra
    juce_recommended_config_flags
)
