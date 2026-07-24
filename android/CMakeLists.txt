cmake_minimum_required(VERSION 3.20)

project(NamAppAndroid)

# Include the main DSP core library
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/core)

# We would also pull in Oboe here
# FetchContent_Declare(oboe ...)

add_library(nam-jni SHARED
    src/JniBridge.cpp
)

# target_link_libraries(nam-jni PRIVATE DSPCore oboe)
