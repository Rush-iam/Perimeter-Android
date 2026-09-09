cmake_minimum_required(VERSION 3.22.1)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/AndroidCompilerCache.cmake")
project("perimeter_android")

find_package(games-frame-pacing REQUIRED CONFIG)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/AndroidDependencySources.cmake")

set(PERIMETER_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../Perimeter")
set(PERIMETER_SOURCE_DIR "${PERIMETER_ROOT}/Source")

# Configure core project options for Android. DXVK is the preferred default
# for hardware with Vulkan support; Sokol remains available as a fallback.
option(PERIMETER_ANDROID_DXVK "Build the Android DXVK D3D9 renderer" ON)
set(OPTION_FFMPEG_MOVIES ON CACHE BOOL "" FORCE)
set(OPTION_DISABLE_STACKTRACE OFF CACHE BOOL "" FORCE)
set(OPTION_SOKOL ON CACHE BOOL "" FORCE)
set(OPTION_SOKOL_API "GLES3" CACHE STRING "" FORCE)
# Android never ships the desktop script compiler or its host-only script
# processing targets.  Keep this Android-only switch explicit so upstream
# desktop configurations retain their existing defaults.
set(PERIMETER_ANDROID_SKIP_DEVELOPMENT_TARGETS ON CACHE BOOL
    "Skip Android-inapplicable Perimeter development targets")

# Create a proxy for 'GL' so the core project doesn't need modification
# On Android, 'GL' (desktop) doesn't exist, we use GLESv3
add_library(GL INTERFACE)
target_link_libraries(GL INTERFACE GLESv3)

# XTool uses Boost.Stacktrace header-only; compile it with the Android toolchain
# rather than depending on ABI-specific Boost binaries. Keep the complete header
# tree for its transitive dependencies, without configuring Boost's libraries.
perimeter_android_dependency_source_args(_boost_headers_source boost_headers boost_headers)
FetchContent_Declare(
        boost_headers
        URL https://archives.boost.io/release/1.85.0/source/boost_1_85_0.tar.bz2
        URL_HASH SHA256=7009fe1faa1697476bdc7027703a2badb84e849b7b0baad5086b087b971f8617
        SOURCE_SUBDIR perimeter_headers_only
        ${_boost_headers_source}
)
FetchContent_MakeAvailable(boost_headers)
# Supply the core project's find_package(Boost) with the fetched headers.
set(Boost_NO_BOOST_CMAKE ON)
set(Boost_NO_SYSTEM_PATHS ON)
set(Boost_INCLUDE_DIR "${boost_headers_SOURCE_DIR}" CACHE PATH "Boost headers" FORCE)

perimeter_android_dependency_source_args(_sdl2_source SDL2 sdl2)
FetchContent_Declare(
        SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-2.30.7
        GIT_SHALLOW TRUE
        ${_sdl2_source}
)
perimeter_android_dependency_source_args(_sdl2_net_source SDL2_net sdl2_net)
FetchContent_Declare(
        SDL2_net
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_net.git
        GIT_TAG release-2.2.0
        GIT_SHALLOW TRUE
        ${_sdl2_net_source}
)
perimeter_android_dependency_source_args(_sdl2_mixer_source SDL2_mixer sdl2_mixer)
FetchContent_Declare(
        SDL2_mixer
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
        GIT_TAG release-2.8.0
        GIT_SHALLOW TRUE
        ${_sdl2_mixer_source}
)
perimeter_android_dependency_source_args(_sdl2_image_source SDL2_image sdl2_image)
FetchContent_Declare(
        SDL2_image
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
        GIT_TAG release-2.8.2
        GIT_SHALLOW TRUE
        ${_sdl2_image_source}
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
perimeter_android_verify_git_revision(SDL2 "${sdl2_SOURCE_DIR}"
    9519b9916cd29a14587af0507292f2bd31dd5752)
perimeter_android_verify_git_revision(SDL2_net "${sdl2_net_SOURCE_DIR}"
    669e75b84632e2c6cc5c65974ec9e28052cb7a4e)
perimeter_android_verify_git_revision(SDL2_mixer "${sdl2_mixer_SOURCE_DIR}"
    a37e09f85d321a13dfcf0d4432827ee09beeb623)
perimeter_android_verify_git_revision(SDL2_image "${sdl2_image_SOURCE_DIR}"
    abcf63aa71b4e3ac32120fa9870a6500ddcdcc89)

if(PERIMETER_ANDROID_DXVK)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/AndroidDxvk.cmake")
    android_add_dxvk("${sdl2_SOURCE_DIR}/include")
    set(PERIMETER_DXVK_D3D9_TARGET AndroidDxvk::D3D9)
    set(OPTION_D3D9 ON CACHE BOOL "" FORCE)
else()
    set(OPTION_D3D9 OFF CACHE BOOL "" FORCE)
endif()

# FFmpeg Prebuilts for Android
perimeter_android_dependency_source_args(_ffmpeg_prebuilt_source
    ffmpeg_prebuilt ffmpeg_prebuilt)
FetchContent_Declare(
        ffmpeg_prebuilt
        GIT_REPOSITORY https://github.com/hzw1199/Android-FFmpeg-Prebuilt.git
        GIT_TAG 90231cc0105aef4f76926b911535f5eb73511b86
        ${_ffmpeg_prebuilt_source}
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

# Include the core project
# We use EXCLUDE_FROM_ALL to prevent building desktop targets not needed for Android
add_subdirectory(${PERIMETER_ROOT} perimeter_engine EXCLUDE_FROM_ALL)

# RenderSokol compiles independently from the final shared library and needs
# Swappy's public headers while building its Android GLES presentation path.
target_link_libraries(RenderSokol PRIVATE games-frame-pacing::swappy_static)

# Core static libraries compile independently, so they cannot inherit SDL usage
# requirements from the final perimeter shared-library link. Keep those
# requirements scoped to the Android engine targets instead of restoring
# wrapper-global include directories. SDL 2 generates/copies its public headers
# at build time; make every SDL consumer wait for that complete header set before
# compiling, rather than merely before SDL itself links.
add_library(perimeter_android_sdl INTERFACE)
target_link_libraries(perimeter_android_sdl INTERFACE
        SDL2::SDL2
        SDL2_net::SDL2_net
        SDL2_mixer::SDL2_mixer
        SDL2_image::SDL2_image)
# Keep the generated-header ordering edge in this directory. CMake's Ninja
# generator otherwise weakens a cross-directory dependency on SDL's utility
# target to a directory-level order-only edge for static-library objects.
add_custom_target(perimeter_android_sdl_headers
        DEPENDS sdl_headers_copy)
foreach(_perimeter_android_sdl_consumer
        perimeter
        XTool
        XPrmLib
        PluginMAX
        Terra
        tx3d
        Render
        Sound
        Network
        Util
        Units
        UserInterface
        Physics
        AI
        GeoLogic
        HT
        Integrations
        Exodus)
    target_link_libraries(${_perimeter_android_sdl_consumer} PRIVATE
            perimeter_android_sdl)
    add_dependencies(${_perimeter_android_sdl_consumer}
            perimeter_android_sdl_headers)
endforeach()
unset(_perimeter_android_sdl_consumer)

# Add Android-specific logging redirector to the engine target
target_sources(perimeter PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/AndroidLog.cpp")

# Link the core engine library and dependencies
target_link_libraries(
        perimeter PRIVATE
        SDL2::SDL2
        SDL2_net::SDL2_net
        SDL2_mixer::SDL2_mixer
        SDL2_image::SDL2_image
        ffmpeg
        games-frame-pacing::swappy_static
        android
        EGL
        log)
