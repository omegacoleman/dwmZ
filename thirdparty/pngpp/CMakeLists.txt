add_library(pngpp INTERFACE)

target_include_directories(pngpp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pngpp INTERFACE PkgConfig::PNG)

add_library(pngpp::pngpp ALIAS pngpp)

