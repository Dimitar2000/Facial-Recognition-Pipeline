cmake_minimum_required(VERSION 3.31)

project(facial-recog-pipeline)

set(CMAKE_EXPORT_COMPILE_COMMANDS on)

find_package(PkgConfig REQUIRED)

set(OpenCV_DIR "/home/mitko/Libraries/opencv/build" CACHE PATH "Path to the CUDA-enabled OpenCV build")
list(PREPEND CMAKE_MODULE_PATH "${OpenCV_DIR}/../cmake")

find_package(OpenCV 5 REQUIRED CONFIG
    PATHS /usr/local/lib/cmake/opencv5
    NO_DEFAULT_PATH
)

set(PKG_CONFIG_DEPENDENCIES
	gstreamer-1.0
	gstreamer-video-1.0
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
			   src/pipeline/elements/fr_element.cpp
			   src/pipeline/elements/fr_metadata_visualizer.cpp
			   src/pipeline/elements/burstqueue.cpp
			   src/pipeline/metadata/fr_metadata.cpp
			   src/pipeline/webcam_fr_pipeline.cpp
			   src/fr/fr_processor.cpp
			   src/fr/cpu_fr_processor.cpp
			   src/fr/gpu_fr_processor.cpp
			   src/util/yaml.cpp
			   src/pipeline/webcam_fr_pipeline.hpp
			   src/pipeline/mp4_fr_pipeline.hpp
			   src/pipeline/elements/fr_element.hpp
			   src/pipeline/elements/fr_metadata_visualizer.hpp
			   src/util/record.hpp
)

target_include_directories(facial-recog-pipeline PRIVATE src/)
target_include_directories(facial-recog-pipeline PRIVATE ${OpenCV_INCLUDE_DIRS})

target_link_libraries(facial-recog-pipeline PRIVATE ${DEPENDENCY_TARGETS})
target_link_libraries(facial-recog-pipeline PRIVATE ${OpenCV_LIBS})