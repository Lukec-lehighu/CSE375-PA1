cmake_minimum_required(VERSION 3.14)
project(prime-checker CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find oneTBB / Intel TBB package
find_package(TBB REQUIRED)

# For normal pthreads
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Define executable with your single source file
add_executable(prime-checker prime-checker.cpp)

# Link the TBB target (handles include directories, flags, and library linking)
target_link_libraries(prime-checker PRIVATE TBB::tbb PRIVATE Threads::Threads)