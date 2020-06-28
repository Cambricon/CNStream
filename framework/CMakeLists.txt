set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib/)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin/)

set(VERSION_HEAD_FILE ${PROJECT_SOURCE_DIR}/framework/core/include/cnstream_version.hpp)
execute_process(COMMAND awk /CNSTREAM_MAJOR_VERSION/ ${VERSION_HEAD_FILE} OUTPUT_VARIABLE VERSION_STR)
string(REGEX MATCH "[0-9]+"  MAJOR_VERSION ${VERSION_STR})
execute_process(COMMAND awk /CNSTREAM_MINOR_VERSION/ ${VERSION_HEAD_FILE} OUTPUT_VARIABLE VERSION_STR)
string(REGEX MATCH "[0-9]+"  MINOR_VERSION ${VERSION_STR})
execute_process(COMMAND awk /CNSTREAM_PATCH_VERSION/ ${VERSION_HEAD_FILE} OUTPUT_VARIABLE VERSION_STR)
string(REGEX MATCH "[0-9]+"  PATCH_VERSION ${VERSION_STR})
set(CNSTREAM_TARGET_VERSION "${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}")

include_directories(${PROJECT_SOURCE_DIR}/3rdparty/googletest/include/)
include_directories(${3RDPARTY_INCLUDE_DIRS})

if(build_perf)
  include_directories(${PROJECT_SOURCE_DIR}/3rdparty/sqlite3/)
endif()

set(module_list core)

#FIXME
set(SOURCE_LINKER_LIBS dl pthread)

foreach(module ${module_list})
  include_directories(${PROJECT_SOURCE_DIR}/framework/${module}/include)
  file(GLOB_RECURSE module_src ${PROJECT_SOURCE_DIR}/framework/${module}/*.cpp)
  list(APPEND srcs ${module_src})
endforeach()
add_library(cnstream_core SHARED ${srcs})

if (SANITIZE_MEMORY OR SANITIZE_ADDRESS OR SANITIZE_THREAD OR SANITIZE_UNDEFINED)
    add_sanitizers(cnstream_core)
endif()

install(TARGETS cnstream_core LIBRARY DESTINATION lib)
target_link_libraries(cnstream_core ${SOURCE_LINKER_LIBS} sqlite3)
set_target_properties(cnstream_core PROPERTIES VERSION ${CNSTREAM_TARGET_VERSION})
install(DIRECTORY core/include/ DESTINATION include)

if(build_tests)
  add_subdirectory(unitest)
endif()
