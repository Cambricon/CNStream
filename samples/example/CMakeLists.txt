set(Example_DIR "${PROJECT_SOURCE_DIR}/samples")

link_directories("${Example_DIR}/../lib")
link_directories("${Example_DIR}/../mlu/${MLU_PLATFORM}/libs/${CMAKE_SYSTEM_PROCESSOR}/")

list(APPEND Example_INCLUDE_DIRS ${3RDPARTY_INCLUDE_DIRS})
list(APPEND Example_INCLUDE_DIRS "${Example_DIR}/../include")
list(APPEND Example_INCLUDE_DIRS "${Example_DIR}/../framework/core/include")

list(APPEND Example_LINKER_LIBS cnstream_core)
list(APPEND Example_LINKER_LIBS dl ${3RDPARTY_LIBS} pthread)

include_directories("${Example_INCLUDE_DIRS}")
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} srcs)
set(SRC example.cpp)
get_filename_component(name "${SRC}" NAME_WE)
message("target :  ${name}")
add_executable(${name} ${srcs})
add_dependencies(${name} cnstream_core)
target_link_libraries(${name} ${Example_LINKER_LIBS})
