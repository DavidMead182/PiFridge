cmake_minimum_required(VERSION 3.16)

project(pi_fridge_rpi_sensors LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(rpi_sensors_headers INTERFACE)
target_include_directories(rpi_sensors_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(door_light_controller_test
    tests/DoorLightControllerTest.cpp
)
target_link_libraries(door_light_controller_test PRIVATE rpi_sensors_headers)

enable_testing()
add_test(NAME door_light_controller_test COMMAND door_light_controller_test)
