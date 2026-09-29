cmake_minimum_required(VERSION 3.31)

project(facial-recog-pipeline)

set(CMAKE_EXPORT_COMPILE_COMMANDS on)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GS REQUIRED gstreamer-1.0)
pkg_check_modules(GS_VIDEO REQUIRED gstreamer-video-1.0)
pkg_check_modules(OCV REQUIRED opencv4)

add_executable(facial-recog-pipeline main.cpp fr_transformer.cpp debug.cpp)

target_include_directories(facial-recog-pipeline PUBLIC ${GS_INCLUDE_DIRS} ${GS_VIDEO_INCLUDE_DIRS} ${OCV_INCLUDE_DIRS})
target_link_libraries(facial-recog-pipeline ${GS_LIBRARIES} ${GS_VIDEO_LIBRARIES} ${OCV_LIBRARIES})
target_compile_options(facial-recog-pipeline PUBLIC ${GS_CFLAGS_OTHER} ${GS_VIDEO_CFLAGS_OTHER} ${OCV_CFLAGS_OTHER})


