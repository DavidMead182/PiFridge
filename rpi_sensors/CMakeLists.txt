cmake_minimum_required(VERSION 3.16)

project(pi_fridge_rpi_sensors LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(rpi_sensors STATIC
    src/Bh1750Sensor.cpp
)

target_include_directories(rpi_sensors PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(door_light_controller_test
    tests/DoorLightControllerTest.cpp
)

target_link_libraries(door_light_controller_test PRIVATE rpi_sensors)

enable_testing()
add_test(NAME door_light_controller_test COMMAND door_light_controller_test)
