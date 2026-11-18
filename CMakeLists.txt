cmake_minimum_required(VERSION 3.25)

project(echo_mini_fw LANGUAGES C)

if(WIN32)
    enable_language(RC)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ECHO_SANITIZE "Build host code with UndefinedBehaviorSanitizer" OFF)
option(ECHO_ADDRESS_SANITIZER "Build host code with AddressSanitizer" OFF)
option(ECHO_BUILD_SIM "Build the SDL2 simulator" ON)
option(ECHO_BUILD_TESTS "Build the host tests" ON)
option(ECHO_BUILD_DEVICE "Build the device firmware with cmake/arm-none-eabi.cmake" OFF)
option(ECHO_RELEASE "Leave the developer-only Diagnostics rows out of the build" OFF)
option(ECHO_FEATURE_MP4 "Include the MP4 container (m4a/m4b, AAC and ALAC)" ON)
option(ECHO_FEATURE_OGG "Include Ogg/Opus playback" ON)
option(ECHO_FEATURE_BLUETOOTH "Include Bluetooth audio" ON)
option(ECHO_FEATURE_EQUALIZER "Include the equalizer" ON)
option(ECHO_FEATURE_WAV "Include WAV playback and indexing" ON)
option(ECHO_FEATURE_QOA "Include QOA playback and indexing" ON)
option(ECHO_FEATURE_EXTRA_INDEX "Index WMA, APE, DSF and DFF" ON)
option(ECHO_RELEASE_VERSION_ONLY "Show only the firmware version in the release Diagnostics menu" OFF)
set(ECHO_APP_RAM_BUDGET_BYTES 131072 CACHE STRING "Static RAM budget for src/ on the device")

message(STATUS "EchoPod features: MP4=${ECHO_FEATURE_MP4} OGG=${ECHO_FEATURE_OGG} "
               "Bluetooth=${ECHO_FEATURE_BLUETOOTH} Equalizer=${ECHO_FEATURE_EQUALIZER} "
               "WAV=${ECHO_FEATURE_WAV} QOA=${ECHO_FEATURE_QOA} "
               "ExtraIndex=${ECHO_FEATURE_EXTRA_INDEX} "
               "VersionOnly=${ECHO_RELEASE_VERSION_ONLY}")

include(cmake/project_options.cmake)

add_subdirectory(src)
add_subdirectory(platform/device/logic)

if(ECHO_BUILD_SIM)
    add_subdirectory(platform/sim)
endif()

if(ECHO_BUILD_DEVICE)
    add_subdirectory(platform/device)
endif()

if(ECHO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
