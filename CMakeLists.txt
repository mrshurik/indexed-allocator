cmake_minimum_required(VERSION 3.1)

cmake_policy(SET CMP0057 NEW)

project(Indexed)

find_package(Boost 1.60 REQUIRED)
find_package(Threads)

set(CMAKE_CXX_STANDARD 11)

enable_testing()

add_library(indexed INTERFACE)

file(GLOB INCLUDES include/indexed/*.h)

target_sources(indexed INTERFACE
    ${INCLUDES}
)

target_include_directories(indexed INTERFACE
    ${Boost_INCLUDE_DIRS}
    include
)

add_subdirectory(tests)

if(CMAKE_BUILD_TYPE STREQUAL "Release")

add_executable(Bench bench/bench.cpp)

target_link_libraries(Bench PRIVATE
    indexed
    Threads::Threads
)

install(DIRECTORY include DESTINATION include)

endif()
