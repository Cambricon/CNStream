# ---[ Google-gflags
#include("${PROJECT_SOURCE_DIR}/cmake/FindGFlags.cmake")
include_directories(${GFLAGS_INCLUDE_DIRS})

# ---[ Google-glog
#include("${PROJECT_SOURCE_DIR}/cmake/FindGlog.cmake")
include_directories(${GLOG_INCLUDE_DIRS})

include_directories("${PROJECT_SOURCE_DIR}/framework/core/include")

set(SRC cnstream_inspect.cpp)
get_filename_component(name "${SRC}" NAME_WE)
message("target :  ${name}")

set(CMAKE_EXE_LINKER_FLAGS "-Wl,--as-needed")

add_executable(${name} ${SRC})

target_link_libraries(${name} cnstream_va cnstream_core dl glog pthread)

if(build_modules_contrib)
  target_link_libraries(${name} cnstream_contrib)
endif()

