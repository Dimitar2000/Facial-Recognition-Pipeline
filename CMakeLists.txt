cmake_minimum_required(VERSION 3.31)

project(facial-recog-pipeline)

set(CMAKE_EXPORT_COMPILE_COMMANDS on)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GS REQUIRED gstreamer-1.0)

add_executable(facial-recog-pipeline main.cpp)

target_include_directories(facial-recog-pipeline PUBLIC ${GS_INCLUDE_DIRS})
target_link_libraries(facial-recog-pipeline ${GS_LIBRARIES})
target_compile_options(facial-recog-pipeline PUBLIC ${GS_CFLAGS_OTHER})


