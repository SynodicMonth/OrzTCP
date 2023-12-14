cmake_minimum_required(VERSION 3.15)

# set the project name and version
project(OrzTCP VERSION 1.0)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

# include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# Automatically add all source files in the src directory for the sender and receiver executables
# Exclude the main files from the globbing and add them explicitly to their respective executables
file(GLOB_RECURSE PROJECT_SOURCES LIST_DIRECTORIES false ${PROJECT_SOURCE_DIR}/src/*.cpp)
list(REMOVE_ITEM PROJECT_SOURCES "${PROJECT_SOURCE_DIR}/src/sender.cpp")
list(REMOVE_ITEM PROJECT_SOURCES "${PROJECT_SOURCE_DIR}/src/receiver.cpp")
list(REMOVE_ITEM PROJECT_SOURCES "${PROJECT_SOURCE_DIR}/src/bench.cpp")
list(REMOVE_ITEM PROJECT_SOURCES "${PROJECT_SOURCE_DIR}/src/bench_cli.cpp")

# Find the Threads package
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# add the sender executable
add_executable(sender ${PROJECT_SOURCE_DIR}/src/sender.cpp ${PROJECT_SOURCES})
target_link_libraries(sender PUBLIC Threads::Threads)

# add the receiver executable
add_executable(receiver ${PROJECT_SOURCE_DIR}/src/receiver.cpp ${PROJECT_SOURCES})
target_link_libraries(receiver PUBLIC Threads::Threads)

# add the bench executable
add_executable(bench ${PROJECT_SOURCE_DIR}/src/bench.cpp ${PROJECT_SOURCES})
target_link_libraries(bench PUBLIC Threads::Threads)

# add the bench executable
add_executable(bench_cli ${PROJECT_SOURCE_DIR}/src/bench_cli.cpp ${PROJECT_SOURCES})
target_link_libraries(bench_cli PUBLIC Threads::Threads)

# O3

include(CPack)
