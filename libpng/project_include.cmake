# Expose the IDF component through find_package(PNG)
get_filename_component(component_name ${CMAKE_CURRENT_LIST_DIR} NAME)
set(idf_png_library idf::${component_name})
if(NOT TARGET PNG::PNG AND
   (NOT DEFINED CACHE{PNG_LIBRARY} OR "${PNG_LIBRARY}" STREQUAL "${idf_png_library}"))
    set(PNG_PNG_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/libpng"
        CACHE PATH "png.h location provided by the ${component_name} component" FORCE)
    set(PNG_LIBRARY "${idf_png_library}"
        CACHE STRING "libpng library target provided by the ${component_name} component" FORCE)
    mark_as_advanced(PNG_PNG_INCLUDE_DIR PNG_LIBRARY)

    add_library(PNG::PNG INTERFACE IMPORTED)
    set_target_properties(PNG::PNG PROPERTIES
        INTERFACE_LINK_LIBRARIES "${PNG_LIBRARY}")
endif()
