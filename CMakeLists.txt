cmake_minimum_required(VERSION 3.13)

project(dwmz)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)

# set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
# set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")

option(WITH_WP "wireplumber integration volume indicator + hotkeys" OFF)
option(WITH_FCITX "fcitx im indicator" OFF)
option(WITH_UTILITY "build debug utilities(wpvolc, fcitximc, wmbartest)" OFF)

find_package(Threads REQUIRED)
find_package(LibEv REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(Xlib REQUIRED x11 xft xrender xrandr IMPORTED_TARGET)
pkg_check_modules(FreeType2 REQUIRED freetype2 IMPORTED_TARGET)
pkg_check_modules(FontConfig REQUIRED fontconfig IMPORTED_TARGET)
pkg_check_modules(PNG REQUIRED libpng16 IMPORTED_TARGET)

if(${WITH_WP})
  pkg_check_modules(WirePlumber REQUIRED wireplumber-0.5 IMPORTED_TARGET)
endif()

if(${WITH_FCITX})
  pkg_check_modules(DBus REQUIRED dbus-1 IMPORTED_TARGET)
endif()

pkg_check_modules(FontConfig REQUIRED fontconfig IMPORTED_TARGET)

set(SPDLOG_FMT_EXTERNAL ON)
add_subdirectory(thirdparty/fmt EXCLUDE_FROM_ALL)
add_subdirectory(thirdparty/spdlog EXCLUDE_FROM_ALL)
add_subdirectory(thirdparty/utf8proc EXCLUDE_FROM_ALL)
add_subdirectory(thirdparty/agg EXCLUDE_FROM_ALL)
add_subdirectory(thirdparty/date EXCLUDE_FROM_ALL)
add_subdirectory(thirdparty/pngpp EXCLUDE_FROM_ALL)

set(DWMZ_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/include)
add_subdirectory(src)
add_subdirectory(scripts)

