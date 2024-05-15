add_executable(wpvolc wpvolc.cpp)
target_include_directories(wpvolc PUBLIC ${DWMZ_INCLUDE_DIRS})
target_link_libraries(wpvolc PRIVATE
  spdlog::spdlog
  fmt::fmt
  PkgConfig::WirePlumber

  Threads::Threads
)

