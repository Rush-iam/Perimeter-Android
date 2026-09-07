include_guard(GLOBAL)

include(FetchContent)

set(PERIMETER_ANDROID_DEPENDENCY_SOURCE_DIR "" CACHE PATH
    "Optional shared directory populated with Android dependency sources")

if(PERIMETER_ANDROID_DEPENDENCY_SOURCE_DIR)
    get_filename_component(PERIMETER_ANDROID_DEPENDENCY_SOURCE_DIR
        "${PERIMETER_ANDROID_DEPENDENCY_SOURCE_DIR}" ABSOLUTE)
    file(TO_CMAKE_PATH "${PERIMETER_ANDROID_DEPENDENCY_SOURCE_DIR}"
        PERIMETER_ANDROID_DEPENDENCY_SOURCE_DIR)
    file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}/../../../../.artifacts"
        _perimeter_android_artifacts_dir)
    string(FIND "${PERIMETER_ANDROID_DEPENDENCY_SOURCE_DIR}/"
        "${_perimeter_android_artifacts_dir}/" _perimeter_android_artifacts_prefix)
    if(_perimeter_android_artifacts_prefix EQUAL 0)
        message(FATAL_ERROR
            "PERIMETER_ANDROID_DEPENDENCY_SOURCE_DIR must not be under .artifacts")
    endif()
    message(STATUS
        "Android shared dependency sources: ${PERIMETER_ANDROID_DEPENDENCY_SOURCE_DIR}")
endif()

# For a missing shared entry, return a FetchContent SOURCE_DIR argument so the
# first build populates it. For an existing entry, set FetchContent's source
# override so a new CMake build tree skips all population/download steps.
function(perimeter_android_dependency_source_args output dependency_name directory_name)
    if(PERIMETER_ANDROID_DEPENDENCY_SOURCE_DIR)
        string(TOUPPER "${dependency_name}" _dependency_upper)
        set(_override "FETCHCONTENT_SOURCE_DIR_${_dependency_upper}")
        if(DEFINED ${_override} AND NOT "${${_override}}" STREQUAL "")
            set(${output} "" PARENT_SCOPE)
            return()
        endif()

        set(_shared_source
            "${PERIMETER_ANDROID_DEPENDENCY_SOURCE_DIR}/${directory_name}")
        if(EXISTS "${_shared_source}")
            set(${_override} "${_shared_source}" PARENT_SCOPE)
            message(STATUS "Reusing ${dependency_name} source: ${_shared_source}")
            set(${output} "" PARENT_SCOPE)
        else()
            set(${output} SOURCE_DIR "${_shared_source}" PARENT_SCOPE)
        endif()
    else()
        set(${output} "" PARENT_SCOPE)
    endif()
endfunction()

# A shallow clone needs a tag or branch name. Pair those declarations with an
# immutable commit lock so a moved upstream tag fails configuration rather than
# silently changing the dependency.
function(perimeter_android_verify_git_revision dependency source_dir expected_revision)
    if(NOT EXISTS "${source_dir}/.git")
        message(STATUS
            "${dependency}: caller-provided source has no Git metadata; revision check skipped")
        return()
    endif()

    find_package(Git QUIET REQUIRED)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
        WORKING_DIRECTORY "${source_dir}"
        OUTPUT_VARIABLE _actual_revision
        OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL ANY)
    if(NOT _actual_revision STREQUAL expected_revision)
        message(FATAL_ERROR
            "${dependency} resolved to ${_actual_revision}, expected ${expected_revision}. "
            "The upstream release tag may have moved or the shared source directory is stale.")
    endif()
endfunction()

# These are declared by the upstream project, but Android may select a shared
# population destination first. FetchContent's first declaration wins. Keep the
# immutable revisions synchronized with the guarded declarations in Perimeter/.
if(PERIMETER_ANDROID_DEPENDENCY_SOURCE_DIR)
    perimeter_android_dependency_source_args(_simpleini_source simpleini simpleini)
    FetchContent_Declare(simpleini
        GIT_REPOSITORY https://github.com/brofield/simpleini
        GIT_TAG 7350fcc9228f410309734c3fb6dae2bf513cdd98
        ${_simpleini_source})

    perimeter_android_dependency_source_args(_pevents_source pevents pevents)
    FetchContent_Declare(pevents
        GIT_REPOSITORY https://github.com/neosmart/pevents
        GIT_TAG d6afcbc629cf806f6465238849278e530e1d56fb
        ${_pevents_source})

    perimeter_android_dependency_source_args(_gamemath_source gamemath gamemath)
    FetchContent_Declare(gamemath
        GIT_REPOSITORY https://github.com/caiiiycuk/perimeter-gamemath
        GIT_TAG 155ab00471b10b0a1e19c18588e6ccf9a356cb8a
        ${_gamemath_source})

    perimeter_android_dependency_source_args(_sokol_source sokol sokol)
    FetchContent_Declare(sokol
        GIT_REPOSITORY https://github.com/floooh/sokol
        GIT_TAG 4bda1469d3b311af03a34dd956460776c920dc2e
        ${_sokol_source})

    perimeter_android_dependency_source_args(_imgui_source imgui imgui)
    FetchContent_Declare(imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui
        GIT_TAG cb16568fca5297512ff6a8f3b877f461c4323fbe
        ${_imgui_source})
endif()
