cmake_minimum_required(VERSION 3.21)

# apex-dsp: framework-free DSP shared by every Apex plugin (no JUCE).
# Can be built on its own for fast iteration:
#   cmake -S libs/apex-dsp -B build-dsp -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-dsp && ctest --test-dir build-dsp --output-on-failure
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(apex_dsp LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    enable_testing()
endif()

add_library(apex_dsp STATIC
    src/AbyssReverb.cpp
    src/ChugShaper.cpp
    src/Fft.cpp
    src/LowDirt.cpp
    src/LpcFormantCorrector.cpp
    src/PitchShifter.cpp
    src/SpectralShifter.cpp
    src/SplicingShifter.cpp
    src/StereoDelay.cpp)

target_include_directories(apex_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(apex_dsp PUBLIC cxx_std_17)
set_target_properties(apex_dsp PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    target_compile_options(apex_dsp PRIVATE /W4)
else()
    target_compile_options(apex_dsp PRIVATE -Wall -Wextra)
endif()

option(APEX_DSP_BUILD_TESTS "Build the apex-dsp tests and offline tools" ON)
if(APEX_DSP_BUILD_TESTS)
    add_executable(apex_pitch_tests tests/pitch_tests.cpp)
    target_link_libraries(apex_pitch_tests PRIVATE apex_dsp)
    add_test(NAME pitch_engine COMMAND apex_pitch_tests)

    add_executable(apex_fx_tests tests/fx_tests.cpp)
    target_link_libraries(apex_fx_tests PRIVATE apex_dsp)
    add_test(NAME rig_effects COMMAND apex_fx_tests)

    add_executable(apex_pitch_render tests/pitch_render.cpp)
    target_link_libraries(apex_pitch_render PRIVATE apex_dsp)
endif()
