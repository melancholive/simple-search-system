cmake_minimum_required(VERSION 3.20)
project(web_search_engines_2)

set(CMAKE_CXX_STANDARD 17)

add_executable(parser parser.cpp)
add_executable(merger merger.cpp)
add_executable(query query_processor.cpp)