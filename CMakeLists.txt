cmake_minimum_required(VERSION 3.16)
project(gooblebox CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)

# Используем IMPORTED_TARGET - это правильный способ
pkg_check_modules(WLROOTS REQUIRED IMPORTED_TARGET wlroots-0.18)
pkg_check_modules(WAYLAND_SERVER REQUIRED IMPORTED_TARGET wayland-server)
pkg_check_modules(XKBCOMMON REQUIRED IMPORTED_TARGET xkbcommon)

# Отладочная информация
message(STATUS "WLROOTS_VERSION = ${WLROOTS_VERSION}")
message(STATUS "WLROOTS_LIBRARIES = ${WLROOTS_LIBRARIES}")

add_executable(gooblebox src/main.cpp)

target_compile_definitions(gooblebox PRIVATE WLR_USE_UNSTABLE)

# Линкуем через PkgConfig:: - CMake сам добавит нужные флаги
target_link_libraries(gooblebox PRIVATE
    PkgConfig::WLROOTS
    PkgConfig::WAYLAND_SERVER
    PkgConfig::XKBCOMMON
)
