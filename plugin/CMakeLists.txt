cmake_minimum_required(VERSION 3.17)
project(plugin)

set(CMAKE_CXX_STANDARD 17)
set(LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/lib)

add_compile_options(-O3 -fPIC)

find_package(Threads REQUIRED)

add_library(chess OBJECT chess.cpp)
target_link_libraries(chess Threads::Threads)

add_executable(run main.cpp)
target_link_libraries(run chess)

add_library(plugin SHARED plugin.cpp)
target_link_libraries(plugin chess)