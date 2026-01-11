cmake_minimum_required(VERSION 3.10)
project(KeyValueStore)

# Set C++ standard to 17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add the executable
add_executable(fifo_cache fifo_cache.cpp)

# Link pthread for thread support
find_package(Threads REQUIRED)
target_link_libraries(fifo_cache PRIVATE Threads::Threads)

# Find and link SQLite3
find_package(SQLite3 REQUIRED)
target_link_libraries(fifo_cache PRIVATE SQLite::SQLite3)
