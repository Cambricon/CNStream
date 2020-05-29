set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib/)

set(contrib_modules_list cnencode)

foreach(contrib_module ${contrib_modules_list})
  include_directories(${PROJECT_SOURCE_DIR}/modules_contrib/${contrib_module}/include)
  file(GLOB_RECURSE contrib_module_src ${PROJECT_SOURCE_DIR}/modules_contrib/${contrib_module}/*.cpp)
  list(APPEND contrib_module_srcs ${contrib_module_src})
endforeach()

if(DEFINED contrib_modules_list)
  add_library(cnstream_contrib SHARED ${contrib_module_srcs})
  include_directories(${OpenCV_INCLUDE_DIRS})
  target_link_libraries(cnstream_contrib cnrt cnstream-toolkit ${OpenCV_LIBS} ${SOURCE_LINKER_LIBS})
  install(TARGETS cnstream_contrib LIBRARY DESTINATION lib)
endif()
