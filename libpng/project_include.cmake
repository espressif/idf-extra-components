# Expose the IDF component through find_package(PNG).
get_filename_component(component_name ${CMAKE_CURRENT_LIST_DIR} NAME)
set(idf_png_library idf::${component_name})
if(NOT TARGET PNG::PNG AND
   (NOT DEFINED PNG_LIBRARY OR "${PNG_LIBRARY}" STREQUAL "${idf_png_library}"))
    set(PNG_PNG_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/libpng)
    set(PNG_LIBRARY ${idf_png_library})

    add_library(PNG::PNG INTERFACE IMPORTED)
    set_target_properties(PNG::PNG PROPERTIES
        INTERFACE_LINK_LIBRARIES "${PNG_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${PNG_PNG_INCLUDE_DIR}")
endif()
