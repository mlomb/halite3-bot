cmake_minimum_required(VERSION 3.0)

project(MyBot CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O2 -w -pedantic")

include_directories(${CMAKE_SOURCE_DIR}/Code)
include_directories(${CMAKE_SOURCE_DIR}/include)

file(GLOB_RECURSE SOURCE_FILES "Code/*.h" "Code/*.hpp" "Code/*.c" "Code/*.cpp")
add_executable(MyBot ${SOURCE_FILES})