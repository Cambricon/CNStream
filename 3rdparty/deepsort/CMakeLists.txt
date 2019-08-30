option(build_deepsort_demo "build demo" ON)
option(RELEASE "build mode" ON)
# -- using C++11
set(CMAKE_CXX_FLAGS "-std=c++11")
#######################################################################
# -- Release and Debug
if(RELEASE)
    set(CMAKE_BUILD_TYPE Release)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast")
    add_definitions(-DSLOG_SILENCE)
    add_definitions(-DNDEBUG)
else()
    set(CMAKE_BUILD_TYPE Debug)
endif()

#######################################################################

# ---[ Flags
if(UNIX OR APPLE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive -Wall")
endif()

if(USE_libstdcpp)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
  message("-- Warning: forcing libstdc++ (controlled by USE_libstdcpp option in cmake)")
endif()

include_directories(/usr/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/thirdPart)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/matching)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/munkres)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/feature)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/errmsg)
file(GLOB_RECURSE srcs ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
# -- build deepsort
add_definitions(-DFEATURE_MATCH_EN)
add_library(deepsort SHARED ${srcs})

if(build_deepsort_demo)
  add_subdirectory(demo)
endif()
