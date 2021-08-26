cmake_minimum_required(VERSION 3.10.0)
project(quick_client_cmd)

include_directories(/usr/local/include)
aux_source_directory(. SRC_LIST)
set(PATH ../out/)

set(QUICK /root/quick/out/lib/libquick.so)

add_executable(quick_client_cmd ${SRC_LIST})
target_link_libraries(quick_client_cmd pthread ${QUICK} dl)

