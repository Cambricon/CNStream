set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
add_library(sqlite3 STATIC ${CMAKE_CURRENT_SOURCE_DIR}/sqlite3.c)
target_link_libraries(sqlite3 pthread dl)
