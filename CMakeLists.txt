cmake_minimum_required(VERSION 3.10)
project(KeyValueStore)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)

# Unit tests
add_executable(unit_tests tests/tests.cpp)
target_link_libraries(unit_tests
    PRIVATE
        Threads::Threads
        SQLite::SQLite3
)

# Performance tests
add_executable(performance_tests tests/performance_tests.cpp)
target_link_libraries(performance_tests
    PRIVATE
        Threads::Threads
        SQLite::SQLite3
)
