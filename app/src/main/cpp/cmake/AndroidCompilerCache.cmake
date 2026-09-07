# Optional compiler-cache integration for the Android Clang toolchain.  Keep
# the cache outside the build tree so it can be reused by both renderer flavors.
set(ANDROID_COMPILER_CACHE "AUTO" CACHE STRING
    "Android compiler cache: AUTO, CCACHE, SCCACHE, or OFF")
set_property(CACHE ANDROID_COMPILER_CACHE PROPERTY STRINGS AUTO CCACHE SCCACHE OFF)
set(ANDROID_COMPILER_CACHE_EXECUTABLE "" CACHE FILEPATH
    "Explicit ccache or sccache executable for Android builds")
set(ANDROID_COMPILER_CACHE_DIR "" CACHE PATH
    "Shared compiler-cache directory for Android builds")

string(TOUPPER "${ANDROID_COMPILER_CACHE}" _android_compiler_cache_mode)
set(_android_compiler_cache_modes AUTO CCACHE SCCACHE OFF)
if(NOT _android_compiler_cache_mode IN_LIST _android_compiler_cache_modes)
    message(FATAL_ERROR
        "ANDROID_COMPILER_CACHE must be AUTO, CCACHE, SCCACHE, or OFF; got '${ANDROID_COMPILER_CACHE}'.")
endif()

set(_android_compiler_cache_executable "${ANDROID_COMPILER_CACHE_EXECUTABLE}")
set(_android_compiler_cache_kind "")
if(NOT _android_compiler_cache_mode STREQUAL "OFF")
    if(_android_compiler_cache_executable)
        if(NOT EXISTS "${_android_compiler_cache_executable}")
            message(FATAL_ERROR
                "ANDROID_COMPILER_CACHE_EXECUTABLE does not exist: ${_android_compiler_cache_executable}")
        endif()
        get_filename_component(_android_compiler_cache_name
            "${_android_compiler_cache_executable}" NAME_WE)
        string(TOUPPER "${_android_compiler_cache_name}" _android_compiler_cache_kind)
    elseif(_android_compiler_cache_mode STREQUAL "AUTO" OR
           _android_compiler_cache_mode STREQUAL "CCACHE")
        find_program(_android_compiler_cache_executable NAMES ccache)
        if(_android_compiler_cache_executable)
            set(_android_compiler_cache_kind CCACHE)
        endif()
    endif()

    if(NOT _android_compiler_cache_executable AND
       (_android_compiler_cache_mode STREQUAL "AUTO" OR
        _android_compiler_cache_mode STREQUAL "SCCACHE"))
        find_program(_android_compiler_cache_executable NAMES sccache)
        if(_android_compiler_cache_executable)
            set(_android_compiler_cache_kind SCCACHE)
        endif()
    endif()

    if(_android_compiler_cache_executable AND NOT _android_compiler_cache_kind)
        message(FATAL_ERROR
            "ANDROID_COMPILER_CACHE_EXECUTABLE must name ccache or sccache: ${_android_compiler_cache_executable}")
    endif()
    if(NOT _android_compiler_cache_executable AND NOT _android_compiler_cache_mode STREQUAL "AUTO")
        message(FATAL_ERROR
            "Requested ${_android_compiler_cache_mode}, but its executable was not found. Set ANDROID_COMPILER_CACHE_EXECUTABLE or choose AUTO/OFF.")
    endif()
endif()

if(_android_compiler_cache_executable)
    if(NOT ANDROID_COMPILER_CACHE_DIR)
        message(FATAL_ERROR
            "ANDROID_COMPILER_CACHE_DIR must be set when enabling ${_android_compiler_cache_kind}.")
    endif()
    file(MAKE_DIRECTORY "${ANDROID_COMPILER_CACHE_DIR}")
    if(_android_compiler_cache_kind STREQUAL "CCACHE")
        set(_android_compiler_cache_environment "CCACHE_DIR=${ANDROID_COMPILER_CACHE_DIR}")
    else()
        set(_android_compiler_cache_environment "SCCACHE_DIR=${ANDROID_COMPILER_CACHE_DIR}")
    endif()
    # A launcher list lets CMake set the cache directory for every Ninja edge
    # without relying on a developer's shell environment.
    set(CMAKE_C_COMPILER_LAUNCHER "${CMAKE_COMMAND};-E;env;${_android_compiler_cache_environment};${_android_compiler_cache_executable}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CMAKE_COMMAND};-E;env;${_android_compiler_cache_environment};${_android_compiler_cache_executable}")
    message(STATUS "Android compiler cache: ${_android_compiler_cache_kind} (${_android_compiler_cache_executable})")
    message(STATUS "Android compiler cache directory: ${ANDROID_COMPILER_CACHE_DIR}")
else()
    message(STATUS "Android compiler cache: disabled (no ccache or sccache found; set -DANDROID_COMPILER_CACHE=OFF to silence this message)")
endif()

unset(_android_compiler_cache_environment)
unset(_android_compiler_cache_executable)
unset(_android_compiler_cache_kind)
unset(_android_compiler_cache_mode)
unset(_android_compiler_cache_modes)
unset(_android_compiler_cache_name)
