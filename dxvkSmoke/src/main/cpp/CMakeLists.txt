cmake_minimum_required(VERSION 3.22.1)
project(dxvk_smoke LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(FetchContent)
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_VULKAN ON CACHE BOOL "" FORCE)
find_package(games-frame-pacing REQUIRED CONFIG)
FetchContent_Declare(SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-2.30.7)
FetchContent_MakeAvailable(SDL2)

include("${CMAKE_CURRENT_LIST_DIR}/../../../../app/src/main/cpp/cmake/AndroidDependencySources.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../../../../app/src/main/cpp/cmake/AndroidDxvk.cmake")
android_add_dxvk("${sdl2_SOURCE_DIR}/include" games-frame-pacing::swappy_static)
add_library(dxvk_smoke SHARED DxvkSmoke.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../../../../app/src/main/cpp/AndroidLog.cpp")
target_link_libraries(dxvk_smoke PRIVATE AndroidDxvk::D3D9_v2 SDL2::SDL2 android log vulkan)
