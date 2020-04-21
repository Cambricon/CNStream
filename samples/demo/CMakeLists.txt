include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/settings.cmake)
include_directories("${Example_INCLUDE_DIRS}")
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} srcs)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/preprocess preprocess_srcs)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/postprocess postprocess_srcs)
set(SRC demo.cpp)
get_filename_component(name "${SRC}" NAME_WE)
message("target :  ${name}")
if(WITH_RTSP)
  link_directories(${PROJECT_SOURCE_DIR}/3rdparty/live555/lib)
endif()
add_executable(${name} ${srcs} ${preprocess_srcs} ${postprocess_srcs})

if (SANITIZE_MEMORY OR SANITIZE_ADDRESS OR SANITIZE_THREAD OR SANITIZE_UNDEFINED)
    add_sanitizers(${name})
endif()

target_link_libraries(${name} ${Example_LINKER_LIBS})

if(WITH_RTSP AND build_modules_contrib)
  #force load all the modules
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,--no-as-needed")
  target_link_libraries(${name} cnstream_contrib)
endif()
