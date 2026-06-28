cmake_minimum_required(VERSION 3.21)

project(ApexAmp VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ------------------------------------------------------------------
# Options
# ------------------------------------------------------------------
option(APEXAMP_BUILD_TESTS "Build the offline DSP test harness" ON)

# ------------------------------------------------------------------
# JUCE (auto-downloaded via FetchContent so a fresh clone just builds)
# ------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4          # pinned for reproducible builds
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ------------------------------------------------------------------
# The plugin target
# ------------------------------------------------------------------
juce_add_plugin(ApexAmp
    COMPANY_NAME            "PolychromeNext"
    PLUGIN_MANUFACTURER_CODE Pcnx
    PLUGIN_CODE             Apx1
    FORMATS                 VST3 AU Standalone
    PRODUCT_NAME            "ApexAmp"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
)

juce_generate_juce_header(ApexAmp)

target_sources(ApexAmp PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_include_directories(ApexAmp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

target_compile_definitions(ApexAmp PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(ApexAmp PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# ------------------------------------------------------------------
# Offline DSP test harness (pure C++, no JUCE, no audio device needed)
# ------------------------------------------------------------------
if(APEXAMP_BUILD_TESTS)
    enable_testing()
    add_executable(apexamp_offline_test
        tests/offline_test.cpp
    )
    target_include_directories(apexamp_offline_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
    )
    target_compile_features(apexamp_offline_test PRIVATE cxx_std_17)

    add_test(NAME offline_dsp_test COMMAND apexamp_offline_test)
endif()
