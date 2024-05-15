add_executable(fcitximc fcitximc.cpp)
target_include_directories(fcitximc PUBLIC ${DWMZ_INCLUDE_DIRS})
target_link_libraries(fcitximc PRIVATE
  spdlog::spdlog
  fmt::fmt
  PkgConfig::DBus

  Threads::Threads
)

