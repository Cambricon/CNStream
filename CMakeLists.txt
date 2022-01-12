# ----------------------------------------------------------------------------
#  Root CMake file for CNStream
# ----------------------------------------------------------------------------

# Disable in-source builds to prevent source tree corruption.
if(" ${CMAKE_SOURCE_DIR}" STREQUAL " ${CMAKE_BINARY_DIR}")
  message(FATAL_ERROR "Please create a separate directory for build files.")
endif()

cmake_minimum_required(VERSION 2.8.7)
if(POLICY CMP0046)
  cmake_policy(SET CMP0046 NEW)
endif()
if(POLICY CMP0054)
  cmake_policy(SET CMP0054 NEW)
endif()

# ---[ CNStream project
project(CNStream C CXX)
set(CMAKE_VERBOSE_MAKEFILE OFF)

# ---[ Options
option(build_modules "build modules" ON)
option(build_modules_contrib "build extra modules" OFF)
option(build_samples "build sample programs" ON)
option(build_tests "build unit tests" ON)
option(build_test_coverage  "Test code coverage" OFF)
option(build_tools "build tools" ON)
option(build_python_api "build python api" OFF)

# To use sanitizers, the version of GCC is required to be no less than 4.9
option(SANITIZE_MEMORY "Enable MemorySanitizer for sanitized targets." OFF)
option(SANITIZE_ADDRESS "Enable AddressSanitizer for sanitized targets." OFF)
option(SANITIZE_THREAD "Enable ThreadSanitizer for sanitized targets." OFF)
option(SANITIZE_UNDEFINED "Enable UndefinedBehaviorSanitizer for sanitized targets." OFF)

# ---[ Build Flags
if(USE_libstdcpp)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
  message("-- Warning: forcing libstdc++ (controlled by USE_libstdcpp option in cmake)")
endif()
if(build_test_coverage)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
endif()

if(build_tests)
  # ---[ gtest
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/googletest)
  set(build_gtest_already ON)  # used by subdirectories
endif()

# ---[ build
add_subdirectory(framework)
if(build_modules OR build_tools)
  set(LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/easydk/lib)
  add_subdirectory(easydk)
endif()
if(build_modules)
  add_subdirectory(modules)
endif()
if(build_modules_contrib)
  add_subdirectory(modules_contrib)
endif()
if(build_samples)
  add_subdirectory(samples)
endif()
if(build_tools)
  add_subdirectory(tools)
endif()

if(build_python_api)
  add_subdirectory(python)
endif()

# ---[ install
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")  #set runtime path
if(INSTALL_SOURCE)
  install(DIRECTORY easydk DESTINATION . USE_SOURCE_PERMISSIONS
          PATTERN "easydk/lib" EXCLUDE)
endif()
