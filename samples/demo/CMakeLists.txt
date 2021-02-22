set(Example_DIR "${PROJECT_SOURCE_DIR}/samples")

link_directories("${Example_DIR}/../lib")

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../modules")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../modules/util/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../modules/inference/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../modules/inference2/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../modules/osd/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../modules/source/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../modules/encode/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../modules/track/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../modules/display/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../modules/ipc/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../modules/rtsp_sink/include")

list(APPEND Example_INCLUDE_DIRS ${3RDPARTY_INCLUDE_DIRS})
list(APPEND Example_INCLUDE_DIRS "${Example_DIR}/demo")
list(APPEND Example_INCLUDE_DIRS "${Example_DIR}/../include")
list(APPEND Example_INCLUDE_DIRS "${Example_DIR}/../framework/core/include")

list(APPEND Example_LINKER_LIBS cnstream_va cnstream_core)
list(APPEND Example_LINKER_LIBS dl ${CN_LIBS} ${3RDPARTY_LIBS}  ${OpenCV_LIBS} ${FFMPEG_LIBRARIES} pthread )

include_directories("${Example_INCLUDE_DIRS}")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../modules/source/src")
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} srcs)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/preprocess preprocess_srcs)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/postprocess postprocess_srcs)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/obj_filter obj_filter_srcs)

set(SRC demo.cpp)
get_filename_component(name "${SRC}" NAME_WE)
message("target :  ${name}")
add_executable(${name} ${srcs} ${preprocess_srcs} ${postprocess_srcs} ${obj_filter_srcs})

if (SANITIZE_MEMORY OR SANITIZE_ADDRESS OR SANITIZE_THREAD OR SANITIZE_UNDEFINED)
    add_sanitizers(${name})
endif()

add_dependencies(${name} cnstream_va cnstream_core)
target_link_libraries(${name} ${Example_LINKER_LIBS})


add_executable(multi_pipelines ${CMAKE_CURRENT_SOURCE_DIR}/multi_pipelines/multi_pipelines.cpp util.cpp ${preprocess_srcs} ${postprocess_srcs} ${obj_filter_srcs})
add_dependencies(multi_pipelines cnstream_va cnstream_core)
target_link_libraries(multi_pipelines ${Example_LINKER_LIBS})

add_executable(multi_sources ${CMAKE_CURRENT_SOURCE_DIR}/multi_sources/multi_sources.cpp util.cpp ${preprocess_srcs} ${postprocess_srcs} ${obj_filter_srcs})
add_dependencies(multi_sources cnstream_va cnstream_core)
target_link_libraries(multi_sources ${Example_LINKER_LIBS})

if(build_modules_contrib)
  #force load all the modules
  set(CMAKE_EXE_LINKER_FLAGS "-Wl,--no-as-needed")
  target_link_libraries(${name} cnstream_contrib)
endif()
