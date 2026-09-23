cmake_minimum_required(VERSION 3.22.1)
project(xr_smoke LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenXR REQUIRED CONFIG)

add_library(xr_smoke SHARED XrProbe.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../../../../app/src/main/cpp/AndroidLog.cpp")
target_link_libraries(xr_smoke PRIVATE OpenXR::openxr_loader android log vulkan)
