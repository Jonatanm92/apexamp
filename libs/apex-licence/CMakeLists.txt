# apex-licence: offline Ed25519 licence keys (pure C++, no JUCE).
#
# Release builds pass the seller's public key:
#   cmake -B build -DAPEX_LICENCE_PUBLIC_KEY=<64 hex digits>
# Without it the development key is used (see dev/README.md): never sell such a build.

set(APEX_LICENCE_PUBLIC_KEY "" CACHE STRING "Ed25519 public key (hex) that release licence keys are checked against")

add_library(apex_licence STATIC
    src/Licence.cpp
    third_party/tweetnacl/tweetnacl.c)
target_include_directories(apex_licence
    PUBLIC  include
    PRIVATE third_party/tweetnacl)
set_target_properties(apex_licence PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden)
target_compile_features(apex_licence PUBLIC cxx_std_17)
if(APEX_LICENCE_PUBLIC_KEY)
    if(NOT APEX_LICENCE_PUBLIC_KEY MATCHES "^[0-9a-fA-F]+$")
        message(FATAL_ERROR "APEX_LICENCE_PUBLIC_KEY must be 64 hex digits")
    endif()
    string(LENGTH "${APEX_LICENCE_PUBLIC_KEY}" _apex_key_length)
    if(NOT _apex_key_length EQUAL 64)
        message(FATAL_ERROR "APEX_LICENCE_PUBLIC_KEY must be 64 hex digits")
    endif()
    target_compile_definitions(apex_licence PRIVATE APEX_LICENCE_PUBLIC_KEY="${APEX_LICENCE_PUBLIC_KEY}")
    message(STATUS "apex-licence: release public key")
else()
    message(STATUS "apex-licence: DEVELOPMENT public key (do not sell this build)")
endif()

add_executable(apex_keygen tools/apex_keygen.cpp)
target_link_libraries(apex_keygen PRIVATE apex_licence)

add_executable(apex_licence_tests tests/licence_tests.cpp)
target_include_directories(apex_licence_tests PRIVATE third_party/tweetnacl)
target_link_libraries(apex_licence_tests PRIVATE apex_licence)
add_test(NAME licence_keys COMMAND apex_licence_tests)

# Apex Key Studio (tools/keystudio, a browser page) must make the same keys as
# apex_keygen. Needs Node with Ed25519 in WebCrypto; skipped otherwise.
find_program(APEX_NODE node)
if(APEX_NODE)
    add_test(NAME keystudio_crosscheck
             COMMAND ${APEX_NODE} ${PROJECT_SOURCE_DIR}/tools/keystudio/crosscheck.mjs
                     $<TARGET_FILE:apex_keygen>
                     ${PROJECT_SOURCE_DIR}/tools/keystudio/apex-key-studio.html
                     ${CMAKE_CURRENT_SOURCE_DIR}/dev/development-secret-key.txt)
    set_tests_properties(keystudio_crosscheck PROPERTIES SKIP_RETURN_CODE 77)
endif()
