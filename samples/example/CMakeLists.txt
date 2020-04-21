include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/settings.cmake)
include_directories("${Example_INCLUDE_DIRS}")
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} srcs)
set(SRC example.cpp)
get_filename_component(name "${SRC}" NAME_WE)
message("target :  ${name}")

if(build_rtsp_sink)
  link_directories(${PROJECT_SOURCE_DIR}/3rdparty/live555/lib)
endif()

add_executable(${name} ${srcs})
target_link_libraries(${name} ${Example_LINKER_LIBS})
