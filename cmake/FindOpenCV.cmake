# cmake/FindOpenCV.cmake
# Thin module that:
#  1) Uses OpenCV's CONFIG package (vcpkg provides it)
#  2) Creates a single modern target: opencv::opencv

# IMPORTANT: we're a *module* FindOpenCV.cmake
# Force config mode and avoid recursion into ourselves.
find_package(OpenCV CONFIG REQUIRED NO_MODULE)

# ------------------------------------------------
# Provide a modern target alias: opencv::opencv
# ------------------------------------------------
if(OpenCV_FOUND AND NOT TARGET opencv::opencv)
    add_library(opencv::opencv INTERFACE IMPORTED)

    # Prefer umbrella targets if available, otherwise fall back to legacy vars.
    if(TARGET OpenCV::opencv_world)
        set_target_properties(opencv::opencv PROPERTIES
            INTERFACE_LINK_LIBRARIES "OpenCV::opencv_world"
        )
    elseif(TARGET OpenCV::OpenCV)
        set_target_properties(opencv::opencv PROPERTIES
            INTERFACE_LINK_LIBRARIES "OpenCV::OpenCV"
        )
    elseif(DEFINED OpenCV_LIBS)
        set_target_properties(opencv::opencv PROPERTIES
            INTERFACE_LINK_LIBRARIES "${OpenCV_LIBS}"
        )
    else()
        message(FATAL_ERROR "FindOpenCV.cmake: OpenCV found but no usable targets/libs were exposed.")
    endif()

    # Add include dirs if provided
    if(DEFINED OpenCV_INCLUDE_DIRS)
        set_property(TARGET opencv::opencv APPEND PROPERTY
            INTERFACE_INCLUDE_DIRECTORIES "${OpenCV_INCLUDE_DIRS}")
    endif()
endif()
