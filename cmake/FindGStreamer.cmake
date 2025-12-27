# cmake/FindGStreamer.cmake
# vcpkg (Windows) GStreamer finder without pkg-config.

set(_VCPKG_ROOT "")

# 1) Manifest mode: <build>/vcpkg_installed/<triplet>
if(DEFINED VCPKG_TARGET_TRIPLET AND EXISTS "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}")
  set(_VCPKG_ROOT "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}")
endif()

# 2) If vcpkg toolchain exported VCPKG_INSTALLED_DIR
if(NOT _VCPKG_ROOT AND DEFINED VCPKG_INSTALLED_DIR)
  if(DEFINED VCPKG_TARGET_TRIPLET AND EXISTS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
    set(_VCPKG_ROOT "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
  elseif(EXISTS "${VCPKG_INSTALLED_DIR}")
    set(_VCPKG_ROOT "${VCPKG_INSTALLED_DIR}")
  endif()
endif()

# 3) Classic: %VCPKG_ROOT%/installed/<triplet>
if(NOT _VCPKG_ROOT AND DEFINED ENV{VCPKG_ROOT} AND DEFINED VCPKG_TARGET_TRIPLET AND EXISTS "$ENV{VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}")
  set(_VCPKG_ROOT "$ENV{VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}")
endif()

if(NOT _VCPKG_ROOT OR NOT EXISTS "${_VCPKG_ROOT}")
  message(FATAL_ERROR "FindGStreamer: could not locate vcpkg installed root.")
endif()

# Pick libdir (single-config Ninja)
set(_GST_LIBDIR "${_VCPKG_ROOT}/lib")
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND EXISTS "${_VCPKG_ROOT}/debug/lib")
  set(_GST_LIBDIR "${_VCPKG_ROOT}/debug/lib")
endif()

# Headers (vcpkg layout)
find_path(GSTREAMER_INCLUDE_DIR
  NAMES gst/gst.h
  PATHS "${_VCPKG_ROOT}/include/gstreamer-1.0"
  NO_DEFAULT_PATH
)

# GLib headers required for gst headers to compile
set(_GLIB_INCLUDES
  "${_VCPKG_ROOT}/include/glib-2.0"
  "${_VCPKG_ROOT}/lib/glib-2.0/include"
)

# Libraries
find_library(GSTREAMER_1_0_LIB NAMES gstreamer-1.0 PATHS "${_GST_LIBDIR}" NO_DEFAULT_PATH)
find_library(GSTBASE_1_0_LIB   NAMES gstbase-1.0   PATHS "${_GST_LIBDIR}" NO_DEFAULT_PATH)

find_library(GSTAPP_1_0_LIB    NAMES gstapp-1.0    PATHS "${_GST_LIBDIR}" NO_DEFAULT_PATH)
find_library(GSTVIDEO_1_0_LIB  NAMES gstvideo-1.0  PATHS "${_GST_LIBDIR}" NO_DEFAULT_PATH)
find_library(GSTAUDIO_1_0_LIB  NAMES gstaudio-1.0  PATHS "${_GST_LIBDIR}" NO_DEFAULT_PATH)

find_library(GLIB_2_0_LIB      NAMES glib-2.0      PATHS "${_GST_LIBDIR}" NO_DEFAULT_PATH)
find_library(GOBJECT_2_0_LIB   NAMES gobject-2.0   PATHS "${_GST_LIBDIR}" NO_DEFAULT_PATH)
find_library(GIO_2_0_LIB       NAMES gio-2.0       PATHS "${_GST_LIBDIR}" NO_DEFAULT_PATH)

# Optional (often needed depending on what you use)
find_library(GMODULE_2_0_LIB   NAMES gmodule-2.0   PATHS "${_GST_LIBDIR}" NO_DEFAULT_PATH)
find_library(GTHREAD_2_0_LIB   NAMES gthread-2.0   PATHS "${_GST_LIBDIR}" NO_DEFAULT_PATH)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GStreamer
  REQUIRED_VARS
    GSTREAMER_INCLUDE_DIR
    GSTREAMER_1_0_LIB
    GSTBASE_1_0_LIB
    GLIB_2_0_LIB
    GOBJECT_2_0_LIB
)

if(GStreamer_FOUND)
  set(GSTREAMER_INCLUDE_DIRS
    "${GSTREAMER_INCLUDE_DIR}"
    ${_GLIB_INCLUDES}
  )

  set(GSTREAMER_LIBRARIES
    "${GSTREAMER_1_0_LIB}"
    "${GSTBASE_1_0_LIB}"
    "${GLIB_2_0_LIB}"
    "${GOBJECT_2_0_LIB}"
    "${GIO_2_0_LIB}"
  )

  if(GSTAPP_1_0_LIB)
    list(APPEND GSTREAMER_LIBRARIES "${GSTAPP_1_0_LIB}")
  endif()
  if(GSTVIDEO_1_0_LIB)
    list(APPEND GSTREAMER_LIBRARIES "${GSTVIDEO_1_0_LIB}")
  endif()
  if(GSTAUDIO_1_0_LIB)
    list(APPEND GSTREAMER_LIBRARIES "${GSTAUDIO_1_0_LIB}")
  endif()
  if(GMODULE_2_0_LIB)
    list(APPEND GSTREAMER_LIBRARIES "${GMODULE_2_0_LIB}")
  endif()
  if(GTHREAD_2_0_LIB)
    list(APPEND GSTREAMER_LIBRARIES "${GTHREAD_2_0_LIB}")
  endif()

  # ---- Imported targets (modern CMake style) ----

  if(NOT TARGET gstreamer::gstreamer)
    add_library(gstreamer::gstreamer INTERFACE IMPORTED)
    set_target_properties(gstreamer::gstreamer PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${GSTREAMER_INCLUDE_DIRS}"
      INTERFACE_LINK_LIBRARIES      "${GSTREAMER_LIBRARIES}"
    )
  endif()

  if(NOT TARGET GStreamer::GStreamer)
    add_library(GStreamer::GStreamer INTERFACE IMPORTED)
    set_target_properties(GStreamer::GStreamer PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${GSTREAMER_INCLUDE_DIRS}"
      INTERFACE_LINK_LIBRARIES      "${GSTREAMER_LIBRARIES}"
    )
  endif()
endif()
