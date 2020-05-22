if(build_perf)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -lpthread -ldl")
  add_library(sqlite3 SHARED ${CMAKE_CURRENT_SOURCE_DIR}/sqlite3.c)
endif()

