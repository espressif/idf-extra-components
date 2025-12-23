# idf_create_coverage_report
#
# Create coverage report.
#
# Arguments:
#   report_dir - Directory where the coverage report will be generated
#   SOURCE_DIR - (Optional) Source directory to use as root for gcovr.
#                If not provided, defaults to PROJECT_DIR.
#   GCOV_OPTIONS - (Optional) Additional options to pass to gcovr.
#
# Example:
#   idf_create_coverage_report(${report_dir})  # Uses PROJECT_DIR
#   idf_create_coverage_report(${report_dir} SOURCE_DIR ${component_dir})  # Uses custom source dir
#   idf_create_coverage_report(${report_dir} GCOV_OPTIONS "--gcov-ignore-parse-errors=negative_hits.warn")
#
function(idf_create_coverage_report report_dir)
    cmake_parse_arguments(ARG "" "SOURCE_DIR" "GCOV_OPTIONS" ${ARGN})

    set(gcov_tool ${_CMAKE_TOOLCHAIN_PREFIX}gcov)

    # Use provided SOURCE_DIR or default to PROJECT_DIR
    if(ARG_SOURCE_DIR)
        set(source_root_dir "${ARG_SOURCE_DIR}")
    else()
        idf_build_get_property(source_root_dir PROJECT_DIR)
    endif()

    file(TO_NATIVE_PATH "${report_dir}" _report_dir)
    file(TO_NATIVE_PATH "${source_root_dir}" _source_root_dir)
    file(TO_NATIVE_PATH "${report_dir}/html/index.html" _index_path)

    add_custom_target(pre-cov-report
        COMMENT "Generating coverage report in: ${_report_dir}"
        COMMAND ${CMAKE_COMMAND} -E echo "Using gcov: ${gcov_tool}"
        COMMAND ${CMAKE_COMMAND} -E echo "Source root: ${_source_root_dir}"
        COMMAND ${CMAKE_COMMAND} -E make_directory ${_report_dir}/html
        )

    add_custom_target(gcovr-report
        COMMAND gcovr -r ${_source_root_dir} --gcov-executable ${gcov_tool} ${ARG_GCOV_OPTIONS} -s --html-details ${_index_path}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        DEPENDS pre-cov-report
        )
endfunction()

# idf_clean_coverage_report
#
# Clean coverage report.
function(idf_clean_coverage_report report_dir)
    file(TO_CMAKE_PATH "${report_dir}" _report_dir)

    add_custom_target(cov-data-clean
        COMMENT "Clean coverage report in: ${_report_dir}"
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${_report_dir})
endfunction()
