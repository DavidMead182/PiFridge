cmake_minimum_required(VERSION 3.16)

project(pi_fridge_rpi_sensors LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(pifridge_logic INTERFACE)
target_include_directories(pifridge_logic INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(door_light_controller_test
    tests/DoorLightControllerTest.cpp
)
target_link_libraries(door_light_controller_test PRIVATE pifridge_logic)

enable_testing()
add_test(NAME door_light_controller_test COMMAND door_light_controller_test)

option(PIFRIDGE_BUILD_HARDWARE "Build Raspberry Pi hardware implementations" OFF)

if(PIFRIDGE_BUILD_HARDWARE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GPIOD REQUIRED libgpiod)

    add_library(pifridge_rpi_hw STATIC
        src/Bh1750Sensor.cpp
        src/GpioOutput.cpp
    )

    target_include_directories(pifridge_rpi_hw PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${GPIOD_INCLUDE_DIRS}
    )

    target_link_directories(pifridge_rpi_hw PUBLIC
        ${GPIOD_LIBRARY_DIRS}
    )

    target_link_libraries(pifridge_rpi_hw PUBLIC
        pifridge_logic
        ${GPIOD_LIBRARIES}
    )

    target_compile_options(pifridge_rpi_hw PUBLIC
        ${GPIOD_CFLAGS_OTHER}
    )

    add_executable(pifridge_light_sensor_demo
        src/main.cpp
    )
    target_link_libraries(pifridge_light_sensor_demo PRIVATE pifridge_rpi_hw)
endif()
