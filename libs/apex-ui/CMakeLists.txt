# apex-ui: the Apex design system (JUCE). Shared by every plugin editor.
#
# Sources are compiled into each plugin target (INTERFACE library), so they
# pick up that plugin's JUCE configuration. Fonts are SIL OFL 1.1 (see
# assets/fonts/OFL-*.txt) and are embedded as binary data.

juce_add_binary_data(ApexUiAssets
    NAMESPACE   ApexUiAssets
    HEADER_NAME ApexUiAssets.h
    SOURCES
        assets/fonts/BarlowCondensed-Medium.ttf
        assets/fonts/BarlowCondensed-SemiBold.ttf
        assets/fonts/BarlowCondensed-Bold.ttf
        assets/fonts/BigShouldersDisplay-ExtraBold.ttf
        assets/fonts/JetBrainsMono-Medium.ttf
        assets/fonts/JetBrainsMono-Bold.ttf
        assets/fonts/Cinzel-SemiBold.ttf
        assets/fonts/Cinzel-Bold.ttf
        assets/fonts/CinzelDecorative-Black.ttf)
set_target_properties(ApexUiAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(apex_ui INTERFACE)
target_sources(apex_ui INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Theme.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Materials.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Objects.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/LookAndFeel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Controls.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Shell.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PresetManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorBase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AbyssPaint.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AbyssControls.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Licensing.cpp)
target_include_directories(apex_ui INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(apex_ui INTERFACE ApexUiAssets apex_licence)
