cmake_minimum_required(VERSION 3.10.0)
project(quick)

include_directories(/usr/local/include)
aux_source_directory(. SRC_LIST)
aux_source_directory(./congestion_control CONGESTION_CONTROL)
aux_source_directory(./log LOG)
aux_source_directory(./quick QUICK)
aux_source_directory(./thread THREAD)
aux_source_directory(./thread/linux THREAD_LINUX)
set(PATH ./out/lib)

add_library(quick SHARED ${SRC_LIST} ${CONGESTION_CONTROL} ${LOG} ${QUICK} ${THREAD} ${THREAD_LINUX})
target_link_libraries(quick pthread)

SET(LIBRARY_OUTPUT_PATH ./out/lib)

