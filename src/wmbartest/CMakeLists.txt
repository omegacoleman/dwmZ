add_executable(wmbartest wmbartest.cpp)
target_include_directories(wmbartest PUBLIC ${DWMZ_INCLUDE_DIRS})
target_link_libraries(wmbartest PRIVATE
  spdlog::spdlog
  fmt::fmt
  utf8proc::utf8proc
  agg::agg
  agg::fontfreetype

  PkgConfig::Xlib
  PkgConfig::FreeType2
  PkgConfig::FontConfig

  Threads::Threads
)

