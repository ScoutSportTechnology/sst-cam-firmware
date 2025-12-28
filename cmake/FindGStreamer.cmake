# cmake/FindGStreamer.cmake
# - Try to find GStreamer and its plugins
# Once done, this will define
#
#  GSTREAMER_FOUND - system has GStreamer
#  GSTREAMER_INCLUDE_DIRS - the GStreamer include directories
#  GSTREAMER_LIBRARIES - link these to use GStreamer
#
# Additionally, gstreamer-base is always looked for and required, and
# the following related variables are defined:
#
#  GSTREAMER_BASE_INCLUDE_DIRS - gstreamer-base's include directory
#  GSTREAMER_BASE_LIBRARIES - link to these to use gstreamer-base
#
# Optionally, the COMPONENTS keyword can be passed to find_package()
# and GStreamer plugins can be looked for.  Currently, the following
# plugins can be searched, and they define the following variables if
# found:
#
#  gstreamer-app:        GSTREAMER_APP_INCLUDE_DIRS and GSTREAMER_APP_LIBRARIES
#  gstreamer-audio:      GSTREAMER_AUDIO_INCLUDE_DIRS and GSTREAMER_AUDIO_LIBRARIES
#  gstreamer-fft:        GSTREAMER_FFT_INCLUDE_DIRS and GSTREAMER_FFT_LIBRARIES
#  gstreamer-pbutils:    GSTREAMER_PBUTILS_INCLUDE_DIRS and GSTREAMER_PBUTILS_LIBRARIES
#  gstreamer-video:      GSTREAMER_VIDEO_INCLUDE_DIRS and GSTREAMER_VIDEO_LIBRARIES
#
# Copyright (C) 2012 Raphael Kubo da Costa <rakuco@webkit.org>
#
# BSD license...

find_package(PkgConfig)

# Helper macro to find a GStreamer plugin (or GStreamer itself)
#   _component_prefix is prepended to the _INCLUDE_DIRS and _LIBRARIES variables (eg. "GSTREAMER_AUDIO")
#   _pkgconfig_name is the component's pkg-config name (eg. "gstreamer-1.0", or "gstreamer-video-1.0").
#   _header is the component's header, relative to the gstreamer-1.0 directory (eg. "gst/gst.h").
#   _library is the component's library name (eg. "gstreamer-1.0" or "gstvideo-1.0")
macro(FIND_GSTREAMER_COMPONENT _component_prefix _pkgconfig_name _header _library)
    pkg_check_modules(PC_${_component_prefix} ${_pkgconfig_name})
    find_path(${_component_prefix}_INCLUDE_DIRS
        NAMES ${_header}
        HINTS ${PC_${_component_prefix}_INCLUDE_DIRS} ${PC_${_component_prefix}_INCLUDEDIR}
        PATH_SUFFIXES gstreamer-1.0
    )
    find_library(${_component_prefix}_LIBRARIES
        NAMES ${_library}
        HINTS ${PC_${_component_prefix}_LIBRARY_DIRS} ${PC_${_component_prefix}_LIBDIR}
    )
endmacro()

# ------------------------
# 1. Find GStreamer itself
# ------------------------

# 1.1. Find headers and libraries
FIND_GSTREAMER_COMPONENT(GSTREAMER      gstreamer-1.0      gst/gst.h          gstreamer-1.0)
FIND_GSTREAMER_COMPONENT(GSTREAMER_BASE gstreamer-base-1.0 gst/gst.h          gstbase-1.0)

# 1.2. Check GStreamer version
if (GSTREAMER_INCLUDE_DIRS)
    if (EXISTS "${GSTREAMER_INCLUDE_DIRS}/gst/gstversion.h")
        file(READ "${GSTREAMER_INCLUDE_DIRS}/gst/gstversion.h" GSTREAMER_VERSION_CONTENTS)
        string(REGEX MATCH "#define +GST_VERSION_MAJOR +\\(([0-9]+)\\)" _dummy "${GSTREAMER_VERSION_CONTENTS}")
        set(GSTREAMER_VERSION_MAJOR "${CMAKE_MATCH_1}")
        string(REGEX MATCH "#define +GST_VERSION_MINOR +\\(([0-9]+)\\)" _dummy "${GSTREAMER_VERSION_CONTENTS}")
        set(GSTREAMER_VERSION_MINOR "${CMAKE_MATCH_1}")
        string(REGEX MATCH "#define +GST_VERSION_MICRO +\\(([0-9]+)\\)" _dummy "${GSTREAMER_VERSION_CONTENTS}")
        set(GSTREAMER_VERSION_MICRO "${CMAKE_MATCH_1}")
        set(GSTREAMER_VERSION "${GSTREAMER_VERSION_MAJOR}.${GSTREAMER_VERSION_MINOR}.${GSTREAMER_VERSION_MICRO}")
    endif ()
endif ()

# FIXME: With CMake 2.8.3 we can just pass GSTREAMER_VERSION to FIND_PACKAGE_HANDLE_STANDARD_ARGS as VERSION_VAR
#        and remove the version check here (GSTREAMER_FIND_VERSION would be passed to FIND_PACKAGE).
set(VERSION_OK TRUE)
if (GSTREAMER_FIND_VERSION_EXACT)
    if (NOT(("${GSTREAMER_FIND_VERSION}" VERSION_EQUAL "${GSTREAMER_VERSION}")))
        set(VERSION_OK FALSE)
    endif ()
else ()
    if ("${GSTREAMER_VERSION}" VERSION_LESS "${GSTREAMER_FIND_VERSION}")
        set(VERSION_OK FALSE)
    endif ()
endif ()

# -------------------------
# 2. Find GStreamer plugins
# -------------------------
FIND_GSTREAMER_COMPONENT(GSTREAMER_APP     gstreamer-app-1.0     gst/app/gstappsink.h      gstapp-1.0)
FIND_GSTREAMER_COMPONENT(GSTREAMER_AUDIO   gstreamer-audio-1.0   gst/audio/audio.h         gstaudio-1.0)
FIND_GSTREAMER_COMPONENT(GSTREAMER_FFT     gstreamer-fft-1.0     gst/fft/gstfft.h          gstfft-1.0)
FIND_GSTREAMER_COMPONENT(GSTREAMER_PBUTILS gstreamer-pbutils-1.0 gst/pbutils/pbutils.h     gstpbutils-1.0)
FIND_GSTREAMER_COMPONENT(GSTREAMER_VIDEO   gstreamer-video-1.0   gst/video/video.h         gstvideo-1.0)

# -------------------------
# 2.5 Find GLib (required on Windows for g_free/g_error_free etc.)
# -------------------------
FIND_GSTREAMER_COMPONENT(GLIB    glib-2.0    glib.h         glib-2.0)
FIND_GSTREAMER_COMPONENT(GOBJECT gobject-2.0 glib-object.h  gobject-2.0)
FIND_GSTREAMER_COMPONENT(GIO     gio-2.0     gio/gio.h      gio-2.0)

# ------------------------------------------------
# 3. Process the COMPONENTS passed to FIND_PACKAGE
# ------------------------------------------------
set(_GSTREAMER_REQUIRED_VARS
    GSTREAMER_INCLUDE_DIRS GSTREAMER_LIBRARIES
    VERSION_OK
    GSTREAMER_BASE_INCLUDE_DIRS GSTREAMER_BASE_LIBRARIES
    GLIB_INCLUDE_DIRS GLIB_LIBRARIES
    GOBJECT_INCLUDE_DIRS GOBJECT_LIBRARIES
    GIO_INCLUDE_DIRS GIO_LIBRARIES
)

foreach (_component ${GStreamer_FIND_COMPONENTS})
    set(_gst_component "GSTREAMER_${_component}")
    string(TOUPPER ${_gst_component} _UPPER_NAME)
    list(APPEND _GSTREAMER_REQUIRED_VARS ${_UPPER_NAME}_INCLUDE_DIRS ${_UPPER_NAME}_LIBRARIES)
endforeach ()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(GStreamer DEFAULT_MSG ${_GSTREAMER_REQUIRED_VARS})

# ------------------------------------------------------------------------
# 3.5 vcpkg-friendly GLib include fix:
# gst/gst.h includes <glib.h>. On Windows/vcpkg, pkg-config may not provide
# GLib include dirs, so add the common vcpkg layout paths if they exist.
# ------------------------------------------------------------------------
if(GSTREAMER_FOUND)
    # GSTREAMER_INCLUDE_DIRS is typically .../include/gstreamer-1.0
    get_filename_component(_gst_inc_root "${GSTREAMER_INCLUDE_DIRS}" DIRECTORY) # -> .../include

    set(_glib_inc_1 "${_gst_inc_root}/glib-2.0")
    set(_glib_inc_2 "${_gst_inc_root}/../lib/glib-2.0/include")

    if(EXISTS "${_glib_inc_1}")
        list(APPEND GSTREAMER_INCLUDE_DIRS "${_glib_inc_1}")
        list(APPEND GLIB_INCLUDE_DIRS "${_glib_inc_1}")
        list(APPEND GOBJECT_INCLUDE_DIRS "${_glib_inc_1}")
        list(APPEND GIO_INCLUDE_DIRS "${_glib_inc_1}")
    endif()
    if(EXISTS "${_glib_inc_2}")
        list(APPEND GSTREAMER_INCLUDE_DIRS "${_glib_inc_2}")
        list(APPEND GLIB_INCLUDE_DIRS "${_glib_inc_2}")
        list(APPEND GOBJECT_INCLUDE_DIRS "${_glib_inc_2}")
        list(APPEND GIO_INCLUDE_DIRS "${_glib_inc_2}")
    endif()

    if(DEFINED GSTREAMER_BASE_INCLUDE_DIRS)
        get_filename_component(_gst_base_inc_root "${GSTREAMER_BASE_INCLUDE_DIRS}" DIRECTORY) # -> .../include

        set(_glib_base_inc_1 "${_gst_base_inc_root}/glib-2.0")
        set(_glib_base_inc_2 "${_gst_base_inc_root}/../lib/glib-2.0/include")

        if(EXISTS "${_glib_base_inc_1}")
            list(APPEND GSTREAMER_BASE_INCLUDE_DIRS "${_glib_base_inc_1}")
        endif()
        if(EXISTS "${_glib_base_inc_2}")
            list(APPEND GSTREAMER_BASE_INCLUDE_DIRS "${_glib_base_inc_2}")
        endif()
    endif()
endif()

# ------------------------------------------------
# 4. Provide a modern target alias: gstreamer::gstreamer
# ------------------------------------------------
if(GSTREAMER_FOUND AND NOT TARGET gstreamer::gstreamer)
    add_library(gstreamer::gstreamer INTERFACE IMPORTED)

    # Base include + lib are required
    set_target_properties(gstreamer::gstreamer PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES
            "${GSTREAMER_INCLUDE_DIRS};${GSTREAMER_BASE_INCLUDE_DIRS};${GLIB_INCLUDE_DIRS};${GOBJECT_INCLUDE_DIRS};${GIO_INCLUDE_DIRS}"
        INTERFACE_LINK_LIBRARIES
            "${GSTREAMER_LIBRARIES};${GSTREAMER_BASE_LIBRARIES};${GLIB_LIBRARIES};${GOBJECT_LIBRARIES};${GIO_LIBRARIES}"
    )

    # If plugin components were found, add them too (safe even if empty)
    if(DEFINED GSTREAMER_APP_LIBRARIES)
        set_property(TARGET gstreamer::gstreamer APPEND PROPERTY
            INTERFACE_INCLUDE_DIRECTORIES "${GSTREAMER_APP_INCLUDE_DIRS}")
        set_property(TARGET gstreamer::gstreamer APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES "${GSTREAMER_APP_LIBRARIES}")
    endif()

    if(DEFINED GSTREAMER_AUDIO_LIBRARIES)
        set_property(TARGET gstreamer::gstreamer APPEND PROPERTY
            INTERFACE_INCLUDE_DIRECTORIES "${GSTREAMER_AUDIO_INCLUDE_DIRS}")
        set_property(TARGET gstreamer::gstreamer APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES "${GSTREAMER_AUDIO_LIBRARIES}")
    endif()

    if(DEFINED GSTREAMER_FFT_LIBRARIES)
        set_property(TARGET gstreamer::gstreamer APPEND PROPERTY
            INTERFACE_INCLUDE_DIRECTORIES "${GSTREAMER_FFT_INCLUDE_DIRS}")
        set_property(TARGET gstreamer::gstreamer APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES "${GSTREAMER_FFT_LIBRARIES}")
    endif()

    if(DEFINED GSTREAMER_PBUTILS_LIBRARIES)
        set_property(TARGET gstreamer::gstreamer APPEND PROPERTY
            INTERFACE_INCLUDE_DIRECTORIES "${GSTREAMER_PBUTILS_INCLUDE_DIRS}")
        set_property(TARGET gstreamer::gstreamer APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES "${GSTREAMER_PBUTILS_LIBRARIES}")
    endif()

    if(DEFINED GSTREAMER_VIDEO_LIBRARIES)
        set_property(TARGET gstreamer::gstreamer APPEND PROPERTY
            INTERFACE_INCLUDE_DIRECTORIES "${GSTREAMER_VIDEO_INCLUDE_DIRS}")
        set_property(TARGET gstreamer::gstreamer APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES "${GSTREAMER_VIDEO_LIBRARIES}")
    endif()
endif()

# Optional: provide the more common namespace too
if(GSTREAMER_FOUND AND NOT TARGET GStreamer::GStreamer)
    add_library(GStreamer::GStreamer ALIAS gstreamer::gstreamer)
endif()
