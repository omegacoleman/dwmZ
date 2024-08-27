add_executable(setdwmzpanel setdwmzpanel.cpp)
target_include_directories(setdwmzpanel PUBLIC ${DWMZ_INCLUDE_DIRS})
target_link_libraries(setdwmzpanel PRIVATE
  spdlog::spdlog
  fmt::fmt

  PkgConfig::Xlib
  Threads::Threads
)

