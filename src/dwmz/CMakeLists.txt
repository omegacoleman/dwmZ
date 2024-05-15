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

if(${WITH_WP})
  target_link_libraries(dwmz PRIVATE PkgConfig::WirePlumber)
else()
  target_compile_definitions(dwmz PRIVATE DWMZ_NO_WP)
endif()

if(${WITH_FCITX})
  target_link_libraries(dwmz PRIVATE PkgConfig::DBus)
else()
  target_compile_definitions(dwmz PRIVATE DWMZ_NO_FCITX)
endif()

install(TARGETS dwmz)

