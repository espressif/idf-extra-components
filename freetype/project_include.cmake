# Expose the IDF component through find_package(Freetype).
get_filename_component(component_name ${CMAKE_CURRENT_LIST_DIR} NAME)
set(idf_freetype_library idf::${component_name})
if(NOT TARGET Freetype::Freetype AND
   (NOT DEFINED CACHE{FREETYPE_LIBRARY} OR "${FREETYPE_LIBRARY}" STREQUAL "${idf_freetype_library}"))
    set(FREETYPE_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/freetype/include")
    set(FREETYPE_INCLUDE_DIR_ft2build "${FREETYPE_INCLUDE_DIR}"
        CACHE PATH "ft2build.h location provided by the ${component_name} component" FORCE)
    set(FREETYPE_INCLUDE_DIR_freetype2 "${FREETYPE_INCLUDE_DIR}"
        CACHE PATH "freetype public headers provided by the ${component_name} component" FORCE)
    set(FREETYPE_LIBRARY "${idf_freetype_library}"
        CACHE STRING "FreeType library target provided by the ${component_name} component" FORCE)
    mark_as_advanced(FREETYPE_INCLUDE_DIR_ft2build FREETYPE_INCLUDE_DIR_freetype2 FREETYPE_LIBRARY)

    add_library(Freetype::Freetype INTERFACE IMPORTED)
    set_target_properties(Freetype::Freetype PROPERTIES
        INTERFACE_LINK_LIBRARIES "${FREETYPE_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${FREETYPE_INCLUDE_DIR}")
endif()
