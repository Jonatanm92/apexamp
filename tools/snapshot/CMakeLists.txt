# Headless screenshot tools for UI review (not shipped).
#   cmake -B build -DAPEX_BUILD_SNAPSHOTS=ON && cmake --build build --target apex_ui_gallery
#   xvfb-run ./build/tools/snapshot/apex_ui_gallery out.png 2

juce_add_console_app(apex_ui_gallery PRODUCT_NAME "apex_ui_gallery")
target_sources(apex_ui_gallery PRIVATE Gallery.cpp)
target_compile_definitions(apex_ui_gallery PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(apex_ui_gallery PRIVATE
    apex_ui
    juce::juce_audio_utils
    juce::juce_gui_basics
    juce::juce_graphics
    PUBLIC juce::juce_recommended_config_flags)

# Real editors rendered headlessly:
#   xvfb-run ./apexamp_snapshot out.png [scale] [--set id=value ...] [--tuner]
juce_add_console_app(apexamp_snapshot PRODUCT_NAME "apexamp_snapshot")
target_sources(apexamp_snapshot PRIVATE PluginSnapshot.cpp ${APEXAMP_SOURCES})
target_include_directories(apexamp_snapshot PRIVATE ${APEXAMP_SOURCE_DIR})
target_compile_definitions(apexamp_snapshot PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 NAM_ENABLE_A2_FAST=1 JUCE_MODAL_LOOPS_PERMITTED=1
    APEX_SNAPSHOT_HEADER="PluginProcessor.h" APEX_SNAPSHOT_PROCESSOR=ApexAmpProcessor
    APEX_SNAPSHOT_UNLOCK_HEADER="ThallbyssalEditor.h" APEX_SNAPSHOT_UNLOCK_EDITOR=ThallbyssalEditor
    APEX_STORE_URL="https://example.com/buy")
target_link_libraries(apexamp_snapshot PRIVATE
    ApexAmpAssets apex_dsp apex_ui
    $<LINK_LIBRARY:WHOLE_ARCHIVE,nam_core>
    juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags)
if(UNIX AND NOT APPLE)
    target_link_libraries(apexamp_snapshot PRIVATE stdc++fs)
endif()

juce_add_console_app(apexdrop_snapshot PRODUCT_NAME "apexdrop_snapshot")
target_sources(apexdrop_snapshot PRIVATE PluginSnapshot.cpp ${APEXDROP_SOURCES})
target_include_directories(apexdrop_snapshot PRIVATE ${APEXDROP_SOURCE_DIR})
target_compile_definitions(apexdrop_snapshot PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1
    APEX_SNAPSHOT_HEADER="DropProcessor.h" APEX_SNAPSHOT_PROCESSOR=DropProcessor)
target_link_libraries(apexdrop_snapshot PRIVATE
    apex_dsp apex_ui juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags)

# Demo clips rendered through the real ApexAmp processor (see tools/demo):
#   ./apexamp_demo out-folder [--set id=value ...]
juce_add_console_app(apexamp_demo PRODUCT_NAME "apexamp_demo")
target_sources(apexamp_demo PRIVATE ${PROJECT_SOURCE_DIR}/tools/demo/DemoRender.cpp ${APEXAMP_SOURCES})
target_include_directories(apexamp_demo PRIVATE ${APEXAMP_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/tools/demo)
target_compile_definitions(apexamp_demo PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 NAM_ENABLE_A2_FAST=1 JUCE_MODAL_LOOPS_PERMITTED=1 APEX_STORE_URL="")
target_link_libraries(apexamp_demo PRIVATE
    ApexAmpAssets apex_dsp apex_ui
    $<LINK_LIBRARY:WHOLE_ARCHIVE,nam_core>
    juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags)
if(UNIX AND NOT APPLE)
    target_link_libraries(apexamp_demo PRIVATE stdc++fs)
endif()
