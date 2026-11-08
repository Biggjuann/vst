cmake_minimum_required(VERSION 3.22)

project(HomebrewSynths VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Link the MSVC runtime statically so the .vst3 runs on any Windows machine
# without needing a separate Visual C++ redistributable.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# JUCE is downloaded at configure time. To use a local checkout instead, pass
#   -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# Header-only DSP + UI code shared by every synth in this repo.
add_library(synth_shared INTERFACE)
target_include_directories(synth_shared INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/shared)

add_subdirectory(plugins/Squelch)
add_subdirectory(plugins/Mira)
add_subdirectory(plugins/Octo)
add_subdirectory(plugins/Nona)

option(HOMEBREW_BUILD_TESTS "Build developer test tools" OFF)
if(HOMEBREW_BUILD_TESTS)
    add_subdirectory(tests)
endif()
