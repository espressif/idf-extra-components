cmake_minimum_required(VERSION 3.16)

# add all the components from the root directory of the repo
set(root_dir ${CMAKE_CURRENT_LIST_DIR}/../../..)
file(GLOB component_cmakelists_files ${root_dir}/*/CMakeLists.txt)

# convert to absolute paths
set(component_dirs)
set(component_names)
foreach(component_cmakelists_file ${component_cmakelists_files})
    get_filename_component(component_dir ${component_cmakelists_file} DIRECTORY)
    get_filename_component(component_name ${component_dir} NAME)
    # skip touch_element because it does not support esp32
    if(${component_name} STREQUAL "touch_element")
        continue()
    endif()
    if(${component_name} STREQUAL "test_app")
        continue()
    endif()
    list(APPEND component_names ${component_name})
    list(APPEND component_dirs ${component_dir})
endforeach()

set(EXTRA_COMPONENT_DIRS ${component_dirs})

# generate an idf.py argument file with `--exclude-paths <submodule_path>` for each submodule
set(clang_check_args "--include-paths ${root_dir}")
list(APPEND clang_check_args "--exclude-paths ${root_dir}/.github")
list(APPEND clang_check_args "--exclude-paths $ENV{IDF_PATH}")

# get the list of submodules
execute_process(
    COMMAND git config --file ${root_dir}/.gitmodules --get-regexp path
    OUTPUT_VARIABLE submodules_output
)

# each line will be in the format "submodule.<name>.path <path>", extract the part after the last space
STRING(REGEX REPLACE "\r?\n" ";" submodules_output "${submodules_output}")
foreach(line ${submodules_output})
    # extract the path
    STRING(REGEX REPLACE ".* " "" submodule_path "${line}")
    # add the exclude-paths argument
    list(APPEND clang_check_args "--exclude-paths ${root_dir}/${submodule_path}")
endforeach()

set(clang_check_args_file ${CMAKE_CURRENT_BINARY_DIR}/clang_check_args)
list(JOIN clang_check_args " " clang_check_args_str)
file(WRITE ${clang_check_args_file} ${clang_check_args_str})

# actual project definition...
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(test_app)
