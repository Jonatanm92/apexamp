cmake_minimum_required(VERSION 3.21)

project(ApexAmp VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ------------------------------------------------------------------
# Options
# ------------------------------------------------------------------
option(APEXAMP_BUILD_TESTS "Build the offline DSP test harness" ON)

# Enable low-latency ASIO in the Standalone app on Windows. ASIO requires the
# Steinberg ASIO SDK, which cannot be redistributed, so this is opt-in: download
# the SDK and configure with:
#   cmake -B build -DAPEXAMP_ENABLE_ASIO=ON -DASIO_SDK_DIR=C:/path/to/asiosdk
option(APEXAMP_ENABLE_ASIO "Enable ASIO support (Windows; requires ASIO_SDK_DIR)" OFF)
set(ASIO_SDK_DIR "" CACHE PATH "Path to the Steinberg ASIO SDK root (the folder containing 'common')")

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

# ------------------------------------------------------------------
# Optional ASIO (low-latency Windows audio in the Standalone build)
# ------------------------------------------------------------------
if(APEXAMP_ENABLE_ASIO)
    if(EXISTS "${ASIO_SDK_DIR}/common/iasiodrv.h")
        message(STATUS "ApexAmp: ASIO enabled (SDK: ${ASIO_SDK_DIR})")
        target_compile_definitions(ApexAmp PUBLIC JUCE_ASIO=1)
        target_include_directories(ApexAmp PRIVATE "${ASIO_SDK_DIR}/common")
    else()
        message(WARNING
            "APEXAMP_ENABLE_ASIO is ON but the ASIO SDK was not found at "
            "'${ASIO_SDK_DIR}'. Set -DASIO_SDK_DIR to the SDK root "
            "(the folder containing 'common/iasiodrv.h'). Building without ASIO.")
    endif()
endif()

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
