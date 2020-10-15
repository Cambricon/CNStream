set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/3rdparty/lib)

add_subdirectory(sqlite3)
if(with_web_visualize)
  set(PYBIND11_PYTHON_VERSION 3.5)
  add_subdirectory(pybind11)
endif()
add_subdirectory(googletest)
