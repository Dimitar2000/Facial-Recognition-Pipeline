cmake_minimum_required(VERSION 3.31)

project(facial-recog-pipeline)

set(CMAKE_EXPORT_COMPILE_COMMANDS on)

find_package(PkgConfig REQUIRED)
set(PKG_CONFIG_DEPENDENCIES
	gstreamer-1.0
	gstreamer-video-1.0
	opencv4
)

set(DEPENDENCY_TARGETS)
foreach(dependency IN LISTS PKG_CONFIG_DEPENDENCIES)
	string(MAKE_C_IDENTIFIER "${dependency}" dependency_id)
	string(TOUPPER "${dependency_id}" dependency_id)
	pkg_check_modules(${dependency_id} REQUIRED IMPORTED_TARGET "${dependency}")
	list(APPEND DEPENDENCY_TARGETS PkgConfig::${dependency_id})
endforeach()

add_executable(facial-recog-pipeline 
			   src/main.cpp 
			   src/pipeline/fr_pipeline.cpp
			   src/pipeline/mp4_fr_pipeline.cpp
			   src/pipeline/fr_transformer.cpp 
			   src/pipeline/fr_detection_metadata.cpp
			   src/pipeline/webcam_fr_pipeline.cpp
			   src/util/yaml.cpp
			   src/pipeline/webcam_fr_pipeline.hpp
			   src/pipeline/mp4_fr_pipeline.hpp
)

target_include_directories(facial-recog-pipeline PRIVATE src/)

target_link_libraries(facial-recog-pipeline PRIVATE ${DEPENDENCY_TARGETS})


