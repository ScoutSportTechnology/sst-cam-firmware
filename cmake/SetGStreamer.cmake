function(sst_cam_firmware_gstreamer_runtime target)
  if(NOT TARGET ${target})
    message(FATAL_ERROR "sst_cam_firmware_gstreamer_runtime: target '${target}' does not exist")
  endif()

  if(NOT DEFINED VCPKG_TARGET_TRIPLET OR VCPKG_TARGET_TRIPLET STREQUAL "")
    message(FATAL_ERROR "sst_cam_firmware_gstreamer_runtime: VCPKG_TARGET_TRIPLET is not set")
  endif()

  set(_prefix "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}")

  if(WIN32)
    set(_rel_plugins "plugins/gstreamer")
    set(_rel_bin     "bin")
  else()
    set(_rel_plugins "lib/gstreamer-1.0")
  endif()

  set(_plugins_src
    "$<$<CONFIG:Debug>:${_prefix}/debug/${_rel_plugins}>$<$<NOT:$<CONFIG:Debug>>:${_prefix}/${_rel_plugins}>"
  )

  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/gstreamer-plugins"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${_plugins_src}"
            "$<TARGET_FILE_DIR:${target}>/gstreamer-plugins"
    VERBATIM
  )

  if(WIN32)
    set(_bin_src
      "$<$<CONFIG:Debug>:${_prefix}/debug/${_rel_bin}>$<$<NOT:$<CONFIG:Debug>>:${_prefix}/${_rel_bin}>"
    )

    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory
              "${_bin_src}"
              "$<TARGET_FILE_DIR:${target}>"
      VERBATIM
    )
  endif()
endfunction()
