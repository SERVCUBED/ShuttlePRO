
# Copyright 2013 Eric Messick (FixedImagePhoto.com/Contact)
# Copyright 2018 Albert Graef (aggraef@gmail.com)
# Copyright 2019 Ben Blain (servc.eu/contact)

cmake_minimum_required(VERSION 3.14)
project(ShuttlePRO C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -W -Wall -lX11 -lXtst")

find_package(PkgConfig REQUIRED)
pkg_check_modules(XKB xkbcommon xkbcommon-x11)

add_executable(ShuttlePRO shuttle.h shuttlepro.c)
target_link_libraries(ShuttlePRO ${XKB_LIBRARIES})
include_directories(ShuttlePRO ${XKB_INCLUDE_DIRS})