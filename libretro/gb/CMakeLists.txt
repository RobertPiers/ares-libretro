add_library(ares_gb_libretro SHARED libretro.cpp libretro.h)
add_library(ares::libretro::gb ALIAS ares_gb_libretro)

target_include_directories(ares_gb_libretro PRIVATE ${CMAKE_SOURCE_DIR})

target_compile_definitions(ares_gb_libretro PRIVATE MIA_LIBRARY)

target_link_libraries(
  ares_gb_libretro
  PRIVATE
    ares::ares
    ares::mia
    ares::nall
)

set_target_properties(
  ares_gb_libretro
  PROPERTIES
    OUTPUT_NAME ares_gameboy_libretro
)
