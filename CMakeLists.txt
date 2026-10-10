cmake_minimum_required(VERSION 3.20)
project(web_search_engines_2)

set(CMAKE_CXX_STANDARD 17)

add_executable(parser parser.cpp)
# merger.cpp and query_processor.cpp are empty right now, so they have no main()
# and fail to link. Uncomment each line once that file has a main().
# add_executable(merger merger.cpp)
# add_executable(query query_processor.cpp)