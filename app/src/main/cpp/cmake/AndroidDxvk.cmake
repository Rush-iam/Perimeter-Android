include_guard(GLOBAL)
include(FetchContent)
include(ExternalProject)

# Validate a host executable before persisting it in this Android CMake cache.
# A configured value may have originated from a caller, host discovery, or a
# previous configure, so rechecking it makes stale cache entries fail clearly.
function(_dxvk_validate_host_tool label variable)
    set(_tool_path "${${variable}}")
    if(NOT _tool_path OR NOT EXISTS "${_tool_path}")
        message(FATAL_ERROR
            "${label} path '${_tool_path}' is missing. Set ${variable} to a working host executable.")
    endif()

    set(_tool_command "${_tool_path}")
    if(label STREQUAL "Meson" AND _tool_path MATCHES "\\.[Pp][Yy]$")
        if(NOT ANDROID_DXVK_PYTHON OR NOT EXISTS "${ANDROID_DXVK_PYTHON}")
            message(FATAL_ERROR
                "Meson entry point '${_tool_path}' is a Python script, but ANDROID_DXVK_PYTHON is not usable.")
        endif()
        set(_tool_command "${ANDROID_DXVK_PYTHON}" "${_tool_path}")
    endif()

    execute_process(
        COMMAND ${_tool_command} --version
        RESULT_VARIABLE _tool_result
        OUTPUT_VARIABLE _tool_stdout
        ERROR_VARIABLE _tool_stderr
        TIMEOUT 15)
    string(STRIP "${_tool_stdout}${_tool_stderr}" _tool_output)
    string(REGEX MATCH "^[^\\r\\n]+" _tool_version "${_tool_output}")
    if(NOT _tool_result EQUAL 0 OR NOT _tool_version)
        message(FATAL_ERROR
            "${label} at '${_tool_path}' cannot run '--version' (exit ${_tool_result}). "
            "Set ${variable} to a compatible host tool. Output: ${_tool_output}")
    endif()

    if(label STREQUAL "Python" AND NOT _tool_output MATCHES "[Pp]ython 3\\.")
        message(FATAL_ERROR
            "Python at '${_tool_path}' is incompatible: '${_tool_version}'. "
            "DXVK requires Python 3; set ANDROID_DXVK_PYTHON to a Python 3 executable.")
    endif()
    if(label STREQUAL "Meson")
        string(REGEX MATCH "[0-9]+(\\.[0-9]+)+" _meson_version "${_tool_output}")
        if(NOT _meson_version OR _meson_version VERSION_LESS "0.58.0")
            message(FATAL_ERROR
                "Meson at '${_tool_path}' is incompatible: '${_tool_version}'. "
                "The Android DXVK 2 build requires Meson 0.58 or newer; set ANDROID_DXVK_MESON accordingly.")
        endif()
    endif()

    set(${variable} "${_tool_path}" CACHE FILEPATH "Validated Android DXVK ${label} executable" FORCE)
    set(${variable}_VERSION "${_tool_version}" PARENT_SCOPE)
endfunction()

# Internal helper to find/fetch host tools (Python 3, Meson, glslang).
# Caller values win; then use host discovery; then use the pinned shared-source
# fallback for tools that have one.
macro(_dxvk_ensure_host_tools)
    # 1. Python 3 - Required for Meson and patching
    if(NOT ANDROID_DXVK_PYTHON)
        # Use find_package for standard discovery, but fallback to find_program
        find_package(Python3 QUIET COMPONENTS Interpreter)
        if(Python3_Interpreter_FOUND)
            set(ANDROID_DXVK_PYTHON "${Python3_EXECUTABLE}")
        else()
            find_program(ANDROID_DXVK_PYTHON NAMES python3 python NO_CACHE)
        endif()
    endif()

    _dxvk_validate_host_tool("Python" ANDROID_DXVK_PYTHON)

    # 2. Meson - Build system for DXVK
    if(NOT ANDROID_DXVK_MESON)
        find_program(_HOST_MESON NAMES meson NO_CACHE)
        if(_HOST_MESON)
            set(ANDROID_DXVK_MESON "${_HOST_MESON}")
        else()
            message(STATUS "Meson not found on host, fetching 1.7.0 source via Git...")
            perimeter_android_dependency_source_args(_meson_source meson_src meson_src)
            FetchContent_Declare(meson_src
                GIT_REPOSITORY https://github.com/mesonbuild/meson.git
                GIT_TAG 1.7.0
                GIT_SHALLOW TRUE
                ${_meson_source}
            )
            FetchContent_GetProperties(meson_src)
            if(NOT meson_src_POPULATED)
                FetchContent_Populate(meson_src)
            endif()
            perimeter_android_verify_git_revision(Meson "${meson_src_SOURCE_DIR}"
                897b6fcdf9adfa87fe60f420e1b483f0f49af7a3)
            set(ANDROID_DXVK_MESON "${meson_src_SOURCE_DIR}/meson.py")
        endif()
    endif()

    _dxvk_validate_host_tool("Meson" ANDROID_DXVK_MESON)

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
            NO_CACHE
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
            perimeter_android_dependency_source_args(_glslang_source
                glslang_bin glslang_bin)
            FetchContent_Declare(glslang_bin
                URL "${_glslang_url}"
                URL_HASH "${_glslang_hash}"
                ${_glslang_source}
            )
            FetchContent_GetProperties(glslang_bin)
            if(NOT glslang_bin_POPULATED)
                FetchContent_Populate(glslang_bin)
            endif()
            find_program(ANDROID_DXVK_GLSLANG NAMES glslangValidator glslang
                PATHS "${glslang_bin_SOURCE_DIR}/bin" "${glslang_bin_SOURCE_DIR}"
                NO_DEFAULT_PATH NO_CACHE
            )
        endif()
    endif()

    _dxvk_validate_host_tool("glslang" ANDROID_DXVK_GLSLANG)

    # Finalize tool paths to CMake-style paths
    foreach(tool ANDROID_DXVK_PYTHON ANDROID_DXVK_MESON ANDROID_DXVK_GLSLANG)
        if(${tool})
            file(TO_CMAKE_PATH "${${tool}}" ${tool})
        endif()
    endforeach()
    message(STATUS "Android DXVK host tools: Python ${ANDROID_DXVK_PYTHON_VERSION} (${ANDROID_DXVK_PYTHON})")
    message(STATUS "Android DXVK host tools: Meson ${ANDROID_DXVK_MESON_VERSION} (${ANDROID_DXVK_MESON})")
    message(STATUS "Android DXVK host tools: glslang ${ANDROID_DXVK_GLSLANG_VERSION} (${ANDROID_DXVK_GLSLANG})")
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

    perimeter_android_dependency_source_args(_android_dxvk_source
        android_dxvk_source
        "android_dxvk_source_v${PERIMETER_ANDROID_DXVK_VERSION}")
    FetchContent_Declare(android_dxvk_source
        GIT_REPOSITORY ${android_dxvk_git_repository}
        GIT_TAG ${android_dxvk_git_tag}
        GIT_SUBMODULES_RECURSE TRUE
        ${_android_dxvk_source}
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

    set(android_dxvk_meson_args
        --buildtype=release
        --wrap-mode=nofallback
        -Dforce_fallback_for=libdisplay-info
        ${android_dxvk_component_args}
        ${android_dxvk_frontend_args}
        "-Dandroid_sdl2_include=${sdl_include_dir}"
        ${android_dxvk_sdl_lib_args}
        ${android_dxvk_is_native_args})

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

    # ExternalProject does not infer dependencies from Meson's machine files or
    # command-line options. Record every configuration input in one generated
    # file so content changes invalidate the configure and build stamps.
    set(android_dxvk_inputs "${dxvk_build}/configure-inputs.txt")
    file(SHA256 "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/patch_android_dxvk.py"
        android_dxvk_patch_sha256)
    string(JOIN "\n" android_dxvk_inputs_content
        "git_repository=${android_dxvk_git_repository}"
        "git_revision=${android_dxvk_git_tag}"
        "generation=${PERIMETER_ANDROID_DXVK_VERSION}"
        "variant=${android_dxvk_variant}"
        "source_dir=${android_dxvk_source_SOURCE_DIR}"
        "abi=${ANDROID_ABI}"
        "android_platform=${ANDROID_PLATFORM}"
        "android_stl=${ANDROID_STL}"
        "ndk=${CMAKE_ANDROID_NDK}"
        "toolchain=${CMAKE_TOOLCHAIN_FILE}"
        "ninja=${CMAKE_MAKE_PROGRAM}"
        "python=${ANDROID_DXVK_PYTHON}"
        "meson=${ANDROID_DXVK_MESON}"
        "glslang=${ANDROID_DXVK_GLSLANG}"
        "patch_sha256=${android_dxvk_patch_sha256}"
        "sdl_include=${sdl_include_dir}"
        "sdl_library_dir=${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
        "meson_args=${android_dxvk_meson_args}"
        "")
    file(CONFIGURE OUTPUT "${android_dxvk_inputs}"
        CONTENT "${android_dxvk_inputs_content}" @ONLY)

    set(android_dxvk_setup_script "${dxvk_build}/run-meson-setup.cmake")
    configure_file("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/run_android_dxvk_meson.cmake.in"
        "${android_dxvk_setup_script}" @ONLY)

    set(dxvk_library "${dxvk_build}/build/src/d3d9/libdxvk_d3d9.so")
    ExternalProject_Add(android_dxvk_build
        SOURCE_DIR "${android_dxvk_source_SOURCE_DIR}"
        BINARY_DIR "${dxvk_build}/build"
        DOWNLOAD_COMMAND ""
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND "${CMAKE_COMMAND}" -P "${android_dxvk_setup_script}"
        BUILD_COMMAND "${CMAKE_MAKE_PROGRAM}" -C <BINARY_DIR>
        INSTALL_COMMAND ""
        BUILD_BYPRODUCTS "${dxvk_library}")

    ExternalProject_Add_Step(android_dxvk_build configure_inputs
        COMMAND "${CMAKE_COMMAND}" -E true
        DEPENDEES patch
        DEPENDERS configure
        DEPENDS
            "${android_dxvk_inputs}"
            "${dxvk_build}/android.cross"
            "${dxvk_build}/host.native"
            "${android_dxvk_setup_script}"
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/patch_android_dxvk.py"
        COMMENT "Checking Android DXVK configuration inputs")

    add_library(AndroidDxvk::D3D9 SHARED IMPORTED GLOBAL)
    set_target_properties(AndroidDxvk::D3D9 PROPERTIES
        IMPORTED_LOCATION "${dxvk_library}"
        INTERFACE_INCLUDE_DIRECTORIES "${android_dxvk_source_SOURCE_DIR}/include/native/directx;${android_dxvk_source_SOURCE_DIR}/include/native/windows")
    if(android_dxvk_is_native)
        target_link_libraries(AndroidDxvk::D3D9 INTERFACE SDL2::SDL2)
    endif()
    add_dependencies(AndroidDxvk::D3D9 android_dxvk_build)
endfunction()
