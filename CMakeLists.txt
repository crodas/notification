cmake_minimum_required (VERSION 2.8)
project (notifd)

include(${CMAKE_CURRENT_SOURCE_DIR}/libs/jansson.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/libs/leveldb.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/libs/libuv.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/libs/http-parser.cmake)


file(GLOB web src/*.c src/web/*.c)

find_package(Threads)
find_library(namo nanomsg)

add_executable(notifd ${jansson} ${level} ${uv} ${HTTP} ${web})

target_link_libraries (notifd ${CMAKE_THREAD_LIBS_INIT} dl nanomsg)
