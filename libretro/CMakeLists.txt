if(NOT OS_LINUX)
  message(STATUS "Skipping libretro targets: only supported on Linux")
  return()
endif()

add_subdirectory(gb)
