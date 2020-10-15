# ---[ Google-gflags
#include("${PROJECT_SOURCE_DIR}/cmake/FindGFlags.cmake")
include_directories(${GFLAGS_INCLUDE_DIRS})

# ---[ Google-glog
#include("${PROJECT_SOURCE_DIR}/cmake/FindGlog.cmake")
include_directories(${GLOG_INCLUDE_DIRS})

include_directories(${PROJECT_SOURCE_DIR}/include)

set(SRC get_model_perfinfo.cpp)
get_filename_component(name "${SRC}" NAME_WE)
message("target :  ${name}")
add_executable(${name} ${SRC})

set(LINK_LIBS cnrt cndrv easydk)
if(MLU STREQUAL MLU220_SOC)
    list(APPEND LINK_LIBS ion)
endif()

target_link_libraries(${name} ${LINK_LIBS} pthread ${GLOG_LIBRARIES} ${GFLAGS_LIBRARIES} dl)

