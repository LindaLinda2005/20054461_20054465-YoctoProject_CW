cmake_minimum_required(VERSION 3.28)
project(libmod_demo C)

set(CMAKE_C_STANDARD 11)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBMODBUS REQUIRED libmodbus)

include_directories(${LIBMODBUS_INCLUDE_DIRS})
link_directories(${LIBMODBUS_LIBRARY_DIRS})

add_executable(libmod_demo main.c)
target_link_libraries(libmod_demo ${LIBMODBUS_LIBRARIES})
