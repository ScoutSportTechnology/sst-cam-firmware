# cmake/FindOpenCV.cmake
# Thin shim:
# - Forces CONFIG mode (so vcpkg's OpenCVConfig.cmake is used)
# - Creates a single modern target: opencv::opencv

# IMPORTANT: We're a *module* FindOpenCV.cmake, so do NOT call find_package(OpenCV)
# without CONFIG/NO_MODULE or you'll recurse into yourself.

find_package(OpenCV CONFIG QUIET)

if(NOT OpenCV_FOUND)
  message(FATAL_ERROR
    "FindOpenCV.cmake: OpenCV not found in CONFIG mode. "
    "If you're using vcpkg, make sure the vcpkg toolchain file is set and OpenCV is installed."
  )
endif()

# Prefer real exported targets if they exist, otherwise fall back to legacy vars.
set(_OPENCV_LINK_ITEMS "")

# Common meta/umbrella targets seen in different OpenCV builds
if(TARGET OpenCV::opencv_world)
  list(APPEND _OPENCV_LINK_ITEMS OpenCV::opencv_world)
elseif(TARGET opencv_world)
  list(APPEND _OPENCV_LINK_ITEMS opencv_world)
elseif(TARGET OpenCV::OpenCV)
  list(APPEND _OPENCV_LINK_ITEMS OpenCV::OpenCV)
endif()

# If we didn't find an umbrella target, use OpenCV_LIBS (can contain libs or targets)
if(NOT _OPENCV_LINK_ITEMS)
  if(DEFINED OpenCV_LIBS)
    foreach(_lib IN LISTS OpenCV_LIBS)
      if(TARGET "${_lib}")
        list(APPEND _OPENCV_LINK_ITEMS "${_lib}")
      else()
        list(APPEND _OPENCV_LINK_ITEMS "${_lib}")
      endif()
    endforeach()
  endif()
endif()

# Ensure OpenCV_INCLUDE_DIRS exists (most configs set it)
if(NOT DEFINED OpenCV_INCLUDE_DIRS)
  set(OpenCV_INCLUDE_DIRS "")
endif()

# Provide a single modern target
if(NOT TARGET opencv::opencv)
  add_library(opencv::opencv INTERFACE IMPORTED)
  set_target_properties(opencv::opencv PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${OpenCV_INCLUDE_DIRS}"
    INTERFACE_LINK_LIBRARIES      "${_OPENCV_LINK_ITEMS}"
  )
endif()
