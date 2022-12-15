cmake_minimum_required(VERSION 3.5)

# ---[ eco-solution project
project(CNStream C CXX)
#######################################################################
# -- Build Flags

if (NOT PLATFORM)
  set(PLATFORM "MLU370" CACHE STRING "Build target platform")
endif()

option(BUILD_MODULES "build modules" ON)
option(BUILD_MODULES_CONTRIB "build extra modules" OFF)
option(BUILD_PYTHON_API "Build Python API" OFF)
option(BUILD_SAMPLES "Build samples" ON)
option(BUILD_TESTS "Build all of modules' unit-tests" ON)
option(BUILD_TESTS_COVERAGE  "Build code coverage tests " OFF)
option(BUILD_TOOLS "Build tools" ON)
option(BUILD_PYTHON_API "Build python api" OFF)

option(SANITIZE_MEMORY "Enable MemorySanitizer for sanitized targets." OFF)
option(SANITIZE_ADDRESS "Enable AddressSanitizer for sanitized targets." OFF)
option(SANITIZE_THREAD "Enable ThreadSanitizer for sanitized targets." OFF)
option(SANITIZE_UNDEFINED "Enable UndefinedBehaviorSanitizer for sanitized targets." OFF)

#######################################################################
set(CMAKE_SYSTEM_NAME Linux)

set(CMAKE_INSTALL_DEFAULT_PREFIX "${PROJECT_SOURCE_DIR}/dist")
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_DEFAULT_PREFIX}" CACHE PATH
    "Directory into which to install ${CMAKE_PROJECT_NAME} (default: ${CMAKE_INSTALL_DEFAULT_PREFIX})"
    FORCE)
endif()
message(STATUS "CMAKE_INSTALL_PREFIX = ${CMAKE_INSTALL_PREFIX}")

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake ${PROJECT_SOURCE_DIR}/cmake/sanitizers)
find_package(Sanitizers)

# compile flags
# specify the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Werror -fPIC -D_REENTRANT")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -Wall -fPIC -D_REENTRANT")

set(CMAKE_CXX_FLAGS_DEBUG "-O2 -g")
set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -O2")
set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-as-needed")

if(USE_libstdcpp)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
  message("-- Warning: forcing libstdc++ (controlled by USE_libstdcpp option in cmake)")
endif()

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
message(STATUS "!!!!! BUILD ${CMAKE_BUILD_TYPE} !!!!!")
message(STATUS "!!!!! PLATFORM ${PLATFORM} !!!!")

# ---[ neuware
if (PLATFORM MATCHES "MLU370" OR PLATFORM MATCHES "MLU590")
  find_package(MLU)
  include_directories(${NEUWARE_INCLUDE_DIR})
elseif (PLATFORM MATCHES "CE3226")
  find_package(MPS)
  include_directories(${MPS_INCLUDE_DIR})
else()
  message(FATAL_ERROR "Unsupported PLATFORM: ${PLATFORM}")
endif()

if(BUILD_TESTS_COVERAGE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
endif()

#######################################################################
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake ${PROJECT_SOURCE_DIR}/cmake/sanitizers)
find_package(Sanitizers)

if(BUILD_TESTS)
  # ---[ gtest
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/googletest)
  set(build_gtest_already ON)  # used by subdirectories
endif()

add_subdirectory(easydk)
add_subdirectory(framework)
if(BUILD_MODULES)
  add_subdirectory(modules)
endif()
if(BUILD_MODULES_CONTRIB)
  add_subdirectory(modules_contrib)
endif()
if(BUILD_SAMPLES)
  add_subdirectory(samples)
endif()
if(BUILD_TOOLS)
  add_subdirectory(tools)
endif()

if(BUILD_PYTHON_API)
  add_subdirectory(python)
endif()

