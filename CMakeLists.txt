cmake_minimum_required(VERSION 2.8.11)
project(libcsv)
set(CMAKE_CXX_STANDARD 11)

include_directories(include)

set(SOURCE include/csv.h example/simple_read.cpp)

add_executable(libcsv ${SOURCE})
target_link_libraries(libcsv -lpthread)
