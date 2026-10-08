# ApexAmp — NAM-powered high-gain amp (VST3 / AU / Standalone).
# Built from the suite root; see ../../CMakeLists.txt.

juce_add_plugin(ApexAmp
    VERSION                 0.5.0
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

# NeuralAmpModelerCore: built as its own static library. Each model
# architecture (WaveNet, SlimmableContainer, ...) registers itself with a
# file-scope static initializer. When NAM is linked normally from a static
# archive, the linker DROPS those object files because nothing references them
# directly -> get_dsp() then throws "No config parser registered for
# architecture: SlimmableContainer" and the rig loads as null (silence).
# We therefore link nam_core with WHOLE_ARCHIVE so every object file (and thus
# every self-registration) is force-kept in the final plugin binaries.
set(NAM_DIR ${PROJECT_SOURCE_DIR}/third_party/NeuralAmpModelerCore)
file(GLOB NAM_SOURCES "${NAM_DIR}/NAM/*.cpp" "${NAM_DIR}/NAM/*/*.cpp")

add_library(nam_core STATIC ${NAM_SOURCES})
target_compile_features(nam_core PUBLIC cxx_std_20)
target_compile_definitions(nam_core PUBLIC NAM_ENABLE_A2_FAST=1)
target_include_directories(nam_core PUBLIC
    ${NAM_DIR}
    ${NAM_DIR}/Dependencies/eigen
    ${NAM_DIR}/Dependencies/nlohmann)
set_target_properties(nam_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(APEXAMP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/PluginProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/ThallbyssalEditor.cpp)
set(APEXAMP_SOURCES ${APEXAMP_SOURCES} PARENT_SCOPE)
set(APEXAMP_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Source PARENT_SCOPE)

target_sources(ApexAmp PRIVATE ${APEXAMP_SOURCES})

target_include_directories(ApexAmp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source)

target_compile_definitions(ApexAmp PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    NAM_ENABLE_A2_FAST=1)

target_compile_features(ApexAmp PRIVATE cxx_std_20)

target_link_libraries(ApexAmp PRIVATE
    ApexAmpAssets
    apex_dsp
    apex_ui
    $<LINK_LIBRARY:WHOLE_ARCHIVE,nam_core>
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags)

if(UNIX AND NOT APPLE)
    target_link_libraries(ApexAmp PRIVATE stdc++fs)
endif()

apex_enable_asio(ApexAmp)

# ------------------------------------------------------------------
# Regression guard: prove the NAM rigs actually load and make sound.
# This catches the linker-stripping bug (registrations dropped -> silent
# plugin) before any binary is published.
# ------------------------------------------------------------------
option(APEXAMP_BUILD_TESTS "Build the NAM rig runtime audio check" ON)
if(APEXAMP_BUILD_TESTS)
    add_executable(rig_runtime_check tools/rig_runtime_check.cpp)
    target_link_libraries(rig_runtime_check PRIVATE
        $<LINK_LIBRARY:WHOLE_ARCHIVE,nam_core>)
    if(UNIX AND NOT APPLE)
        target_link_libraries(rig_runtime_check PRIVATE stdc++fs)
    endif()
    add_test(NAME rig_audio
        COMMAND rig_runtime_check
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    add_executable(resample_check tools/resample_check.cpp)
    add_test(NAME resample_streaming COMMAND resample_check)
endif()
