cmake_minimum_required(VERSION 3.21)

project(ApexAmp VERSION 0.3.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)          # NAM core requires C++20
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optional low-latency ASIO support on Windows (Standalone).
# The Steinberg ASIO SDK is downloaded by CI and its path passed in here.
option(APEXAMP_ENABLE_ASIO "Build the Windows Standalone with ASIO support" OFF)
set(ASIO_SDK_DIR "" CACHE PATH "Path to the Steinberg ASIO SDK")

# ------------------------------------------------------------------
# JUCE (auto-downloaded)
# ------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# ------------------------------------------------------------------
# Plugin
# ------------------------------------------------------------------
juce_add_plugin(ApexAmp
    COMPANY_NAME            "PolychromeNext"
    PLUGIN_MANUFACTURER_CODE Pcnx
    PLUGIN_CODE             Apx2
    FORMATS                 VST3 AU Standalone
    PRODUCT_NAME            "ApexAmp"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    IS_MIDI_EFFECT          FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE)

juce_generate_juce_header(ApexAmp)

# Embed the NAM rigs + IRs into the binary (self-contained product).
juce_add_binary_data(ApexAmpAssets SOURCES
    assets/rig_bite.nam
    assets/rig_body.nam
    assets/rig_edge.nam
    assets/ir_ashen.wav
    assets/ir_meshuggah.wav
    assets/ir_pdi09.wav)

# NeuralAmpModelerCore: compile its sources directly into the plugin so the
# architecture-parser static registrations are not stripped by the linker.
set(NAM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/NeuralAmpModelerCore)
file(GLOB NAM_SOURCES "${NAM_DIR}/NAM/*.cpp" "${NAM_DIR}/NAM/*/*.cpp")

target_sources(ApexAmp PRIVATE
    Source/PluginProcessor.cpp
    ${NAM_SOURCES})

target_include_directories(ApexAmp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${NAM_DIR}
    ${NAM_DIR}/Dependencies/eigen
    ${NAM_DIR}/Dependencies/nlohmann)

target_compile_definitions(ApexAmp PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    NAM_ENABLE_A2_FAST=1)

target_compile_features(ApexAmp PRIVATE cxx_std_20)

target_link_libraries(ApexAmp PRIVATE
    ApexAmpAssets
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags)

if(UNIX AND NOT APPLE)
    target_link_libraries(ApexAmp PRIVATE stdc++fs)
endif()

# Enable ASIO in the JUCE audio device layer when the SDK is supplied.
if(APEXAMP_ENABLE_ASIO AND ASIO_SDK_DIR)
    message(STATUS "ApexAmp: ASIO enabled, SDK at ${ASIO_SDK_DIR}")
    target_compile_definitions(ApexAmp PUBLIC JUCE_ASIO=1)
    target_include_directories(ApexAmp PRIVATE "${ASIO_SDK_DIR}/common")
endif()
