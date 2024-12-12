add_executable(dwmz dwm.cpp drw.cpp util.cpp)
target_include_directories(dwmz PUBLIC ${DWMZ_INCLUDE_DIRS})
target_include_directories(dwmz PUBLIC include)
target_link_libraries(dwmz PRIVATE
  spdlog::spdlog
  fmt::fmt
  utf8proc::utf8proc
  date::date
  date::date-tz
  agg::agg
  agg::fontfreetype
  pngpp::pngpp

  LibEv::LibEv
  PkgConfig::Xlib
  PkgConfig::FreeType2
  PkgConfig::FontConfig

  Threads::Threads
)

install(TARGETS dwmz)

