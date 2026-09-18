include_guard(GLOBAL)

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
            "The upstream release tag may have moved or the source directory is stale.")
    endif()
endfunction()
