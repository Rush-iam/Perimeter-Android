cmake_minimum_required(VERSION 3.22.1)

project("perimeter_android")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PERIMETER_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../Perimeter")
set(PERIMETER_SOURCE_DIR "${PERIMETER_ROOT}/Source")
add_definitions(-DPERIMETER_VERSION="3.1.11")
add_definitions(-D_PERIMETER_)

# Configure core project options for Android
set(OPTION_D3D9 OFF CACHE BOOL "" FORCE)
set(OPTION_FFMPEG_MOVIES ON CACHE BOOL "" FORCE)
set(OPTION_DISABLE_STACKTRACE ON CACHE BOOL "" FORCE)
set(OPTION_SOKOL ON CACHE BOOL "" FORCE)
set(OPTION_SOKOL_API "GLES3" CACHE STRING "" FORCE)

# Create a proxy for 'GL' so the core project doesn't need modification
# On Android, 'GL' (desktop) doesn't exist, we use GLESv3
add_library(GL INTERFACE)
target_link_libraries(GL INTERFACE GLESv3)

# SDL2 FetchContent for Android
include(FetchContent)
FetchContent_Declare(
        SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-2.30.7
)
FetchContent_Declare(
        SDL2_net
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_net.git
        GIT_TAG release-2.2.0
)
FetchContent_Declare(
        SDL2_mixer
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
        GIT_TAG release-2.8.0
)
FetchContent_Declare(
        SDL2_image
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
        GIT_TAG release-2.8.2
)

# Disable SDL2 features not needed for engine logic during initial build
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)

# SDL2_mixer options
set(SDL2_MIXER_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDL2_MIXER_MOD OFF CACHE BOOL "" FORCE)
set(SDL2_MIXER_MIDI OFF CACHE BOOL "" FORCE)
set(SDL2_MIXER_OPUS OFF CACHE BOOL "" FORCE)
set(SDL2_MIXER_FLAC OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(SDL2 SDL2_net SDL2_mixer SDL2_image)

# FFmpeg Prebuilts for Android
FetchContent_Declare(
        ffmpeg_prebuilt
        GIT_REPOSITORY https://github.com/hzw1199/Android-FFmpeg-Prebuilt.git
        GIT_TAG main
)
FetchContent_MakeAvailable(ffmpeg_prebuilt)

# The repo has versioned folders
set(FFMPEG_ROOT ${ffmpeg_prebuilt_SOURCE_DIR}/ffmpeg-9.0)
set(FFMPEG_DIR ${FFMPEG_ROOT})

# Define the unified FFmpeg library for Android
add_library(ffmpeg SHARED IMPORTED)
set_target_properties(ffmpeg PROPERTIES
        IMPORTED_LOCATION ${FFMPEG_ROOT}/libffmpeg.so
        INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_ROOT}/include"
)

# Core project uses <SDL.h> but FetchContent target provides <SDL2/SDL.h>
# We add the SDL2 include directory directly to the global include paths
include_directories(${sdl2_SOURCE_DIR}/include)
include_directories(${sdl2_net_SOURCE_DIR})
include_directories(${sdl2_mixer_SOURCE_DIR}/include)
include_directories(${sdl2_image_SOURCE_DIR}/include)

# Include the core project
# We use EXCLUDE_FROM_ALL to prevent building desktop targets not needed for Android
add_subdirectory(${PERIMETER_ROOT} perimeter_engine EXCLUDE_FROM_ALL)

# Add Android-specific logging redirector to the engine target
target_sources(perimeter PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/AndroidLog.cpp")

# Link the core engine library and dependencies
target_link_libraries(
        perimeter PRIVATE
        SDL2::SDL2
        SDL2_net::SDL2_net
        SDL2_mixer::SDL2_mixer
        SDL2_image::SDL2_image
        ffmpeg
        android
        log)
