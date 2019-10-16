cmake_minimum_required(VERSION 2.8.7)
if(POLICY CMP0046)
  cmake_policy(SET CMP0046 NEW)
endif()
if(POLICY CMP0054)
  cmake_policy(SET CMP0054 NEW)
endif()

# ---[ Google-gflags
#include("${PROJECT_SOURCE_DIR}/cmake/FindGFlags.cmake")
include_directories(${GFLAGS_INCLUDE_DIRS})

# ---[ Google-glog
#include("${PROJECT_SOURCE_DIR}/cmake/FindGlog.cmake")
include_directories(${GLOG_INCLUDE_DIRS})

include_directories(${PROJECT_SOURCE_DIR}/include)

file(GLOB tools_srcs ${PROJECT_SOURCE_DIR}/tools/src/*.cpp)

foreach(source_file ${tools_srcs})

  get_filename_component(name ${source_file} NAME_WE)
  message("target :  ${name}")
  add_executable(${name} ${source_file})
  target_link_libraries(${name} cnrt pthread ${GLOG_LIBRARIES} ${GFLAGS_LIBRARIES} z dl)

endforeach()
