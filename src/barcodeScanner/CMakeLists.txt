cmake_minimum_required(VERSION 3.10)
project(BarcodeScanner)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Find CURL
find_package(CURL REQUIRED)

# Library sources
set(SOURCES
    BarcodeScanner.cpp
)

# Create executable
add_executable(main main.cpp ${SOURCES})

# Link libraries
target_link_libraries(main PRIVATE CURL::libcurl)