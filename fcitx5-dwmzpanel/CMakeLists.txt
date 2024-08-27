cmake_minimum_required(VERSION 3.27)

project(fcitx5-dwmzpanel VERSION 0.1.0)

include(GNUInstallDirs)

find_package(Gettext REQUIRED)
find_package(Fcitx5Core 5 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(Xlib REQUIRED x11 IMPORTED_TARGET)

include("${FCITX_INSTALL_CMAKECONFIG_DIR}/Fcitx5Utils/Fcitx5CompilerSettings.cmake")

add_subdirectory(src)

