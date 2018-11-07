find_package(GTest REQUIRED)

set(TEST_SRC
    map_test.cpp
    unordered_test.cpp
    list_test.cpp
    intrusive_test.cpp
    pointer_test.cpp
)

add_executable(indexed_tests ${TEST_SRC})

target_link_libraries(indexed_tests
    GTest::GTest
    GTest::Main
)

target_include_directories(indexed_tests PUBLIC
    ${Boost_INCLUDE_DIRS}
    ../include
)

gtest_add_tests(TARGET indexed_tests)
