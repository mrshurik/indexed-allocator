cmake_minimum_required(VERSION 3.1)

cmake_policy(SET CMP0057 NEW)

project(Indexed)

enable_testing()

set(CMAKE_CXX_STANDARD 11)

file(GLOB INCLUDES include/indexed/*.h)

find_package(Boost 1.60 REQUIRED)
find_package(Threads)

if (NOT NO_INDEXED_TESTS)

add_subdirectory(tests)

endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")

add_executable(Bench ${INCLUDES} bench/bench.cpp)

target_include_directories(Bench PUBLIC
    ${Boost_INCLUDE_DIRS}
    include
)

target_link_libraries (Bench PUBLIC
    Threads::Threads
)

endif()
