cmake_minimum_required(VERSION 3.16)

cmake_minimum_required(VERSION 3.16)
project(PiFridge LANGUAGES CXX)
 
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)
 
add_subdirectory(src)

add_compile_options(-Wall -Wconversion -Wextra -pedantic)