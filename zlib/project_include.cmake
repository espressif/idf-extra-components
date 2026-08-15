# Expose the IDF component through find_package(ZLIB).
get_filename_component(component_name ${CMAKE_CURRENT_LIST_DIR} NAME)
set(idf_zlib_library idf::${component_name})
if(NOT TARGET ZLIB::ZLIB AND
   (NOT DEFINED ZLIB_LIBRARY OR "${ZLIB_LIBRARY}" STREQUAL "${idf_zlib_library}"))
    set(ZLIB_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/zlib)
    set(ZLIB_LIBRARY ${idf_zlib_library})

    add_library(ZLIB::ZLIB INTERFACE IMPORTED)
    set_target_properties(ZLIB::ZLIB PROPERTIES
        INTERFACE_LINK_LIBRARIES "${ZLIB_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${ZLIB_INCLUDE_DIR}")
endif()
