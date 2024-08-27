
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_library(dwmzpanel SHARED dwmzpanel.cpp)
target_link_libraries(dwmzpanel Fcitx5::Core PkgConfig::Xlib)
install(TARGETS dwmzpanel DESTINATION "${CMAKE_INSTALL_LIBDIR}/fcitx5")

fcitx5_translate_desktop_file(${CMAKE_CURRENT_SOURCE_DIR}/dwmzpanel.conf.in dwmzpanel.conf)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/dwmzpanel.conf" DESTINATION "${CMAKE_INSTALL_PREFIX}/share/fcitx5/addon")
