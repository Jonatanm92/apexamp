cmake_minimum_required(VERSION 3.20)
project(NamAppDSPCore VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Fetch NeuralAmpModelerCore
FetchContent_Declare(
    NeuralAmpModelerCore
    GIT_REPOSITORY https://github.com/sdatkinson/NeuralAmpModelerCore.git
    GIT_TAG main
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(NeuralAmpModelerCore)

# Fetch JUCE
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.9
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(juce)

# Collect NAM sources
file(GLOB_RECURSE NAM_SOURCES "${CMAKE_BINARY_DIR}/_deps/neuralampmodelercore-src/NAM/*.cpp")

# DSP Core Library
add_library(DSPCore
    src/DSPCore.cpp
    src/NoiseGate.cpp
    src/CabSimulator.cpp
    src/Tuner.cpp
    src/Metronome.cpp
    src/CloudSyncClient.cpp
    ${NAM_SOURCES}
)

target_include_directories(DSPCore PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
    "${CMAKE_BINARY_DIR}/_deps/neuralampmodelercore-src"
    "${CMAKE_BINARY_DIR}/_deps/neuralampmodelercore-src/Dependencies/eigen"
    "${CMAKE_BINARY_DIR}/_deps/neuralampmodelercore-src/Dependencies/nlohmann"
    "${CMAKE_BINARY_DIR}/_deps/neuralampmodelercore-src/NAM"
    "${CMAKE_BINARY_DIR}/_deps/neuralampmodelercore-src/NAM/wavenet"
)

# Link DSPCore to JUCE
target_link_libraries(DSPCore PRIVATE juce::juce_dsp)

# PC Client (JUCE GUI App / Standalone)
juce_add_gui_app(NamAppClient
    PRODUCT_NAME "NAM App Test Client"
)

target_sources(NamAppClient PRIVATE
    src/Main.cpp
    src/MainComponent.cpp
)

target_link_libraries(NamAppClient PRIVATE
    DSPCore
    juce::juce_gui_extra
    juce::juce_audio_utils
)

# VST3 Plugin
juce_add_plugin(NamAppPlugin
    PLUGIN_MANUFACTURER_CODE "NAMA"
    PLUGIN_CODE "NamA"
    FORMATS VST3 Standalone
    PRODUCT_NAME "NAM App Plugin"
)

target_sources(NamAppPlugin PRIVATE
    src/PluginProcessor.cpp
    src/MainComponent.cpp
)

target_link_libraries(NamAppPlugin PRIVATE 
    DSPCore 
    juce::juce_audio_utils
    juce::juce_audio_processors
)
