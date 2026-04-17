cmake_minimum_required(VERSION 3.16)
project(bh1750 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

add_library(bh1750_logic STATIC
    ILightSensor.cpp
    DoorLightController.cpp
)

target_include_directories(bh1750_logic PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(bh1750 STATIC
    Bh1750Sensor.cpp
)

target_include_directories(bh1750 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(bh1750 PUBLIC
    bh1750_logic
    Threads::Threads
)

enable_testing()

add_executable(door_light_controller_test
    test/DoorLightControllerTest.cpp
)

target_link_libraries(door_light_controller_test PRIVATE
    bh1750_logic
)

add_test(NAME door_light_controller_test COMMAND door_light_controller_test)

add_executable(bh1750_demo
    test/BH1750Demo.cpp
)

target_link_libraries(bh1750_demo PRIVATE
    bh1750
)