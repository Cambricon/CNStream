# ---[ Google-gflags
#include("${PROJECT_SOURCE_DIR}/cmake/FindGFlags.cmake")
include_directories(${GFLAGS_INCLUDE_DIRS})

# ---[ Google-glog
#include("${PROJECT_SOURCE_DIR}/cmake/FindGlog.cmake")
include_directories(${GLOG_INCLUDE_DIRS})

include_directories("${PROJECT_SOURCE_DIR}/modules/core/include")

set(SRC cnstream_inspect.cpp)
get_filename_component(name "${SRC}" NAME_WE)
message("target :  ${name}")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,--no-as-needed")

if(build_rtsp_sink)
  link_directories(${PROJECT_SOURCE_DIR}/3rdparty/live555/lib)
#set(Live555_LIBS liveMedia UsageEnvironment BasicUsageEnvironment groupsock)
# target_link_libraries(${name} ${Live555_LIBS})
endif()

add_executable(${name} ${SRC})

target_link_libraries(${name} cnstream dl glog pthread)
if(build_modules_contrib)
  target_link_libraries(${name} cnstream_contrib)
endif()

