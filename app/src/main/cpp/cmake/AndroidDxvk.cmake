include_guard(GLOBAL)
include(FetchContent)
include(ExternalProject)

# Internal helper to find/fetch host tools (Python 3, Meson, glslang)
macro(_dxvk_ensure_host_tools)
    # 1. Python 3 - Required for Meson and patching
    if(NOT ANDROID_DXVK_PYTHON)
        # Use find_package for standard discovery, but fallback to find_program
        find_package(Python3 QUIET COMPONENTS Interpreter)
        if(Python3_Interpreter_FOUND)
            set(ANDROID_DXVK_PYTHON "${Python3_EXECUTABLE}")
        else()
            find_program(ANDROID_DXVK_PYTHON NAMES python3 python)
        endif()
    endif()

    if(NOT ANDROID_DXVK_PYTHON OR NOT EXISTS "${ANDROID_DXVK_PYTHON}")
        message(FATAL_ERROR "Host Python 3 not found. Please install it and ensure it is in your PATH, or set ANDROID_DXVK_PYTHON.")
    endif()
    set(ANDROID_DXVK_PYTHON "${ANDROID_DXVK_PYTHON}" CACHE FILEPATH "Host Python 3 executable")

    # 2. Meson - Build system for DXVK
    if(NOT ANDROID_DXVK_MESON)
        find_program(_HOST_MESON NAMES meson)
        if(_HOST_MESON)
            set(ANDROID_DXVK_MESON "${_HOST_MESON}")
        else()
            message(STATUS "Meson not found on host, fetching 1.7.0 source via Git...")
            FetchContent_Declare(meson_src
                GIT_REPOSITORY https://github.com/mesonbuild/meson.git
                GIT_TAG 1.7.0
                GIT_SHALLOW TRUE
            )
            FetchContent_GetProperties(meson_src)
            if(NOT meson_src_POPULATED)
                FetchContent_Populate(meson_src)
            endif()
            set(ANDROID_DXVK_MESON "${meson_src_SOURCE_DIR}/meson.py")
        endif()
    endif()

    if(NOT ANDROID_DXVK_MESON OR NOT EXISTS "${ANDROID_DXVK_MESON}")
        message(FATAL_ERROR "Failed to locate Meson. Please set ANDROID_DXVK_MESON manually.")
    endif()
    set(ANDROID_DXVK_MESON "${ANDROID_DXVK_MESON}" CACHE FILEPATH "Host meson entry point")

    # Determine if Meson should be invoked directly or via Python
    if(ANDROID_DXVK_MESON MATCHES "\\.py$")
        set(_DXVK_MESON_COMMAND "${ANDROID_DXVK_PYTHON}" "${ANDROID_DXVK_MESON}")
    else()
        set(_DXVK_MESON_COMMAND "${ANDROID_DXVK_MESON}")
    endif()

    # 3. glslang - Vulkan shader compiler
    if(NOT ANDROID_DXVK_GLSLANG)
        find_program(_HOST_GLSLANG NAMES glslangValidator glslang
            PATHS
                "$ENV{VULKAN_SDK}/Bin"
                "$ENV{VULKAN_SDK}/bin"
                "C:/VulkanSDK/*/Bin"
                "${CMAKE_ANDROID_NDK}/shader-tools/windows-x86_64"
                "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/windows-x86_64/bin"
        )
        if(_HOST_GLSLANG)
            set(ANDROID_DXVK_GLSLANG "${_HOST_GLSLANG}")
        else()
            message(STATUS "glslang not found on host or NDK, fetching 16.5.0 prebuilt...")
            if(CMAKE_HOST_WIN32)
                set(_glslang_url "https://github.com/KhronosGroup/glslang/releases/download/16.5.0/glslang-16.5.0-windows-x86_64-release.zip")
                set(_glslang_hash "SHA256=06b71298b750268c127f2ee7ae0ef7525e2068120c6c8a3a08b2f58ca6f325ce")
            else()
                set(_glslang_url "https://github.com/KhronosGroup/glslang/releases/download/16.5.0/glslang-16.5.0-linux-x86_64-release.zip")
                set(_glslang_hash "SHA256=dfc3fb889eeb9344dce58606a0b37bde89f1eec6140f4e9b4b3d9305f46f64f4")
            endif()
            FetchContent_Declare(glslang_bin
                URL "${_glslang_url}"
                URL_HASH "${_glslang_hash}"
            )
            FetchContent_GetProperties(glslang_bin)
            if(NOT glslang_bin_POPULATED)
                FetchContent_Populate(glslang_bin)
            endif()
            find_program(ANDROID_DXVK_GLSLANG NAMES glslangValidator glslang
                PATHS "${glslang_bin_SOURCE_DIR}/bin" "${glslang_bin_SOURCE_DIR}"
                NO_DEFAULT_PATH
            )
        endif()
    endif()

    if(NOT ANDROID_DXVK_GLSLANG OR NOT EXISTS "${ANDROID_DXVK_GLSLANG}")
        message(FATAL_ERROR "Failed to locate glslangValidator. Please set ANDROID_DXVK_GLSLANG manually.")
    endif()
    set(ANDROID_DXVK_GLSLANG "${ANDROID_DXVK_GLSLANG}" CACHE FILEPATH "Host glslang executable")

    # Finalize tool paths to CMake-style paths
    foreach(tool ANDROID_DXVK_PYTHON ANDROID_DXVK_MESON ANDROID_DXVK_GLSLANG)
        if(${tool})
            file(TO_CMAKE_PATH "${${tool}}" ${tool})
        endif()
    endforeach()
endmacro()

# This target is deliberately independent of Perimeter's desktop DXVK builder.
function(android_add_dxvk sdl_include_dir)
    if(NOT ANDROID OR NOT ANDROID_ABI STREQUAL "arm64-v8a")
        message(FATAL_ERROR "Android DXVK currently supports only Android arm64-v8a")
    endif()
    if(NOT ANDROID_STL STREQUAL "c++_shared")
        message(FATAL_ERROR "Android DXVK requires ANDROID_STL=c++_shared")
    endif()

    _dxvk_ensure_host_tools()

    set(PERIMETER_ANDROID_DXVK_VERSION "2" CACHE STRING "Android DXVK generation (1 or 2)")
    set_property(CACHE PERIMETER_ANDROID_DXVK_VERSION PROPERTY STRINGS 1 2)
    if(NOT PERIMETER_ANDROID_DXVK_VERSION MATCHES "^[12]$")
        message(FATAL_ERROR "PERIMETER_ANDROID_DXVK_VERSION must be 1 or 2")
    endif()

    if(PERIMETER_ANDROID_DXVK_VERSION STREQUAL "1")
        set(android_dxvk_git_repository https://github.com/IonAgorria/dxvk-native)
        set(android_dxvk_git_tag 43aedc756cbd620b9ee8b1cf2c17b17cc49b3781)
    else()
        set(android_dxvk_git_repository https://github.com/doitsujin/dxvk.git)
        set(android_dxvk_git_tag c3dd74be6baec53786d4e064a572185b70347a17)
    endif()

    FetchContent_Declare(android_dxvk_source
        GIT_REPOSITORY ${android_dxvk_git_repository}
        GIT_TAG ${android_dxvk_git_tag}
        GIT_SUBMODULES_RECURSE TRUE
        SOURCE_SUBDIR android_no_cmake)
    FetchContent_MakeAvailable(android_dxvk_source)

    # DXVK 2.x and the native 1.x fork have different Meson interfaces.
    set(android_dxvk_is_native FALSE)
    if(PERIMETER_ANDROID_DXVK_VERSION STREQUAL "1")
        set(android_dxvk_is_native TRUE)
    endif()

    if(EXISTS "${android_dxvk_source_SOURCE_DIR}/meson_options.txt")
        file(READ "${android_dxvk_source_SOURCE_DIR}/meson_options.txt" android_dxvk_options)
        if(android_dxvk_is_native AND NOT android_dxvk_options MATCHES "dxvk_native_force")
            message(FATAL_ERROR "DXVK v1 selected, but the source is not the native fork")
        elseif(NOT android_dxvk_is_native AND android_dxvk_options MATCHES "dxvk_native_force")
            message(FATAL_ERROR "DXVK v2 selected, but the source is the native fork")
        endif()
    endif()

    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/patch_android_dxvk.py")

    set(android_dxvk_variant v2)
    set(android_dxvk_frontend_args -Dnative_sdl2=enabled -Dnative_sdl3=disabled -Dnative_glfw=disabled)
    set(android_dxvk_component_args -Denable_d3d9=true -Denable_d3d8=false -Denable_d3d10=false -Denable_d3d11=false -Denable_dxgi=false)

    if(android_dxvk_is_native)
        set(android_dxvk_variant native)
        set(android_dxvk_is_native_args -Ddxvk_native_force=true -Ddxvk_native_wsi=sdl2)
        set(android_dxvk_frontend_args)
        set(android_dxvk_component_args -Denable_d3d9=true -Denable_tests=false -Denable_dxgi=false -Denable_d3d10=false -Denable_d3d11=false)
        set(android_dxvk_sdl_lib_args "-Dandroid_sdl2_lib=${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
    endif()

    execute_process(COMMAND "${ANDROID_DXVK_PYTHON}"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/patch_android_dxvk.py"
        "${android_dxvk_source_SOURCE_DIR}" "${android_dxvk_variant}"
        COMMAND_ERROR_IS_FATAL ANY)

    set(dxvk_build "${CMAKE_CURRENT_BINARY_DIR}/android-dxvk")
    file(MAKE_DIRECTORY "${dxvk_build}")
    configure_file("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/android-dxvk.cross.in"
        "${dxvk_build}/android.cross" @ONLY)
    configure_file("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/android-dxvk.native.in"
        "${dxvk_build}/host.native" @ONLY)

    set(dxvk_library "${dxvk_build}/build/src/d3d9/libdxvk_d3d9.so")
    ExternalProject_Add(android_dxvk_build
        SOURCE_DIR "${android_dxvk_source_SOURCE_DIR}"
        BINARY_DIR "${dxvk_build}/build"
        DOWNLOAD_COMMAND ""
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND "${CMAKE_COMMAND}" -E env "NINJA=${CMAKE_MAKE_PROGRAM}"
            ${_DXVK_MESON_COMMAND} setup
            <BINARY_DIR> <SOURCE_DIR>
            --reconfigure
            --cross-file "${dxvk_build}/android.cross"
            --native-file "${dxvk_build}/host.native"
            --buildtype=release --wrap-mode=nofallback
            -Dforce_fallback_for=libdisplay-info
            ${android_dxvk_component_args}
            ${android_dxvk_frontend_args}
            "-Dandroid_sdl2_include=${sdl_include_dir}"
            ${android_dxvk_sdl_lib_args}
            ${android_dxvk_is_native_args}
        BUILD_COMMAND "${CMAKE_MAKE_PROGRAM}" -C <BINARY_DIR>
        BUILD_ALWAYS TRUE
        INSTALL_COMMAND ""
        BUILD_BYPRODUCTS "${dxvk_library}")

    add_library(AndroidDxvk::D3D9 SHARED IMPORTED GLOBAL)
    set_target_properties(AndroidDxvk::D3D9 PROPERTIES
        IMPORTED_LOCATION "${dxvk_library}"
        INTERFACE_INCLUDE_DIRECTORIES "${android_dxvk_source_SOURCE_DIR}/include/native/directx;${android_dxvk_source_SOURCE_DIR}/include/native/windows")
    if(android_dxvk_is_native)
        target_link_libraries(AndroidDxvk::D3D9 INTERFACE SDL2::SDL2)
    endif()
    add_dependencies(AndroidDxvk::D3D9 android_dxvk_build)
endfunction()
