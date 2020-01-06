set(Example_DIR "${PROJECT_SOURCE_DIR}/samples")

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../modules/decode/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../modules/inference/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../modules/osd/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../modules/source/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../modules/encode/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../modules/track/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../modules/fps_stats/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../modules/display/include")

link_directories("${Example_DIR}/../lib")
link_directories("${Example_DIR}/../mlu/${MLU_PLATFORM}/libs/${CMAKE_SYSTEM_PROCESSOR}/")

list(APPEND Example_INCLUDE_DIRS ${3RDPARTY_INCLUDE_DIRS})
list(APPEND Example_INCLUDE_DIRS "${Example_DIR}/../include")
list(APPEND Example_INCLUDE_DIRS "${Example_DIR}/../modules/core/include")
list(APPEND Example_INCLUDE_DIRS "${Example_DIR}/../mlu/${MLU_PLATFORM}/include/")

list(APPEND Example_LINKER_LIBS cnstream)
list(APPEND Example_LINKER_LIBS dl ${CN_LIBS} ${3RDPARTY_LIBS}  ${OpenCV_LIBS} ${FFMPEG_LIBRARIES} pthread )
