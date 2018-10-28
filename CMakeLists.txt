cmake_minimum_required(VERSION 2.8)

project(MyBot)

set(BOOST_MATRIX_ADAPTER OFF)
set(MUNKRESCPP_DEVEL_MODE OFF)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O2 -w -pedantic")

include_directories(${CMAKE_SOURCE_DIR}/Code)
include_directories(MyBot munkres/src)

get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)

file(GLOB SOURCE_FILES "Code/*.h" "Code/*.hpp" "Code/*.c" "Code/*.cpp")
add_executable(MyBot ${SOURCE_FILES})