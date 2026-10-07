# Apex Drop — drop-tuning / pitch shifter (VST3 / AU / Standalone).
# Built from the suite root; see ../../CMakeLists.txt. The DSP lives in
# libs/apex-dsp so the same engine can sit in front of ApexAmp's rigs later.

juce_add_plugin(ApexDrop
    VERSION                 0.1.0
    COMPANY_NAME            "PolychromeNext"
    PLUGIN_MANUFACTURER_CODE Pcnx
    PLUGIN_CODE             ApDr
    FORMATS                 VST3 AU Standalone
    PRODUCT_NAME            "Apex Drop"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    IS_MIDI_EFFECT          FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE)

juce_generate_juce_header(ApexDrop)

set(APEXDROP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/DropProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/DropEditor.cpp)
set(APEXDROP_SOURCES ${APEXDROP_SOURCES} PARENT_SCOPE)
set(APEXDROP_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Source PARENT_SCOPE)

target_sources(ApexDrop PRIVATE ${APEXDROP_SOURCES})

target_include_directories(ApexDrop PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source)

target_compile_definitions(ApexDrop PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(ApexDrop PRIVATE
    apex_dsp
    apex_ui
    juce::juce_audio_utils
    PUBLIC
    juce::juce_recommended_config_flags)

apex_enable_asio(ApexDrop)
