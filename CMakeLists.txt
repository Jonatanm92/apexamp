cmake_minimum_required(VERSION 3.21)

# Apex suite: every plugin plus the DSP they share.
#
#   libs/apex-dsp      framework-free DSP (pitch engine), unit tests + offline tools
#   plugins/ApexAmp    NAM-powered high-gain amp
#   plugins/ApexDrop   drop-tuning / pitch shifter
#
# DSP-only build (no JUCE download):  cmake -B build -DAPEX_BUILD_PLUGINS=OFF
# One version for the suite and both plugins, from the VERSION file.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" APEX_VERSION LIMIT_COUNT 1)
project(ApexSuite VERSION ${APEX_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)          # NAM core requires C++20
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

option(APEX_BUILD_PLUGINS "Build the JUCE plugins (downloads JUCE)" ON)

# Optional low-latency ASIO support on Windows (Standalone builds).
# The Steinberg ASIO SDK is downloaded by CI and its path passed in here.
option(APEXAMP_ENABLE_ASIO "Build the Windows Standalone apps with ASIO support" OFF)
set(ASIO_SDK_DIR "" CACHE PATH "Path to the Steinberg ASIO SDK")

# Where the unlock screen's Buy button goes (empty: no Buy button).
set(APEX_STORE_URL "" CACHE STRING "Store page opened by the Buy button")

add_subdirectory(libs/apex-dsp)
add_subdirectory(libs/apex-licence)

if(APEX_BUILD_PLUGINS)
    # --------------------------------------------------------------
    # JUCE (auto-downloaded)
    # --------------------------------------------------------------
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)

    # Enables ASIO in the JUCE audio device layer when the SDK is supplied.
    function(apex_enable_asio target)
        if(APEXAMP_ENABLE_ASIO AND ASIO_SDK_DIR)
            message(STATUS "${target}: ASIO enabled, SDK at ${ASIO_SDK_DIR}")
            target_compile_definitions(${target} PUBLIC JUCE_ASIO=1)
            target_include_directories(${target} PRIVATE "${ASIO_SDK_DIR}/common")
        endif()
    endfunction()

    add_subdirectory(libs/apex-ui)
    add_subdirectory(plugins/ApexAmp)
    add_subdirectory(plugins/ApexDrop)

    option(APEX_BUILD_SNAPSHOTS "Build the headless UI screenshot tools" OFF)
    if(APEX_BUILD_SNAPSHOTS)
        add_subdirectory(tools/snapshot)
    endif()
endif()
