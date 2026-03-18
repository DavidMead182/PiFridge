cmake_minimum_required(VERSION 3.16)

project(pi_fridge_bh1750 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

find_package(Threads REQUIRED)

add_library(pifridge_light_logic STATIC
    ILightSensor.cpp
    DoorLightController.cpp
)

target_include_directories(pifridge_light_logic PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(door_light_controller_test
    test/DoorLightControllerTest.cpp
)

target_link_libraries(door_light_controller_test PRIVATE
    pifridge_light_logic
)

add_test(NAME door_light_controller_test COMMAND door_light_controller_test)

option(PIFRIDGE_BUILD_HARDWARE "Build Raspberry Pi hardware implementations" OFF)

if(PIFRIDGE_BUILD_HARDWARE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GPIOD REQUIRED libgpiod)

    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_INCLUDES ${GPIOD_INCLUDE_DIRS})
    check_cxx_source_compiles("#include <gpiod.h>\nint main(){ (void)&gpiod_chip_request_lines; return 0; }\n" PIFRIDGE_HAVE_GPIOD_V2)
    unset(CMAKE_REQUIRED_INCLUDES)

    add_library(pifridge_rpi_hw STATIC
        Bh1750Sensor.cpp
        GpioOutput.cpp
    )

    target_include_directories(pifridge_rpi_hw PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${GPIOD_INCLUDE_DIRS}
    )

    target_link_directories(pifridge_rpi_hw PUBLIC
        ${GPIOD_LIBRARY_DIRS}
    )

    target_link_libraries(pifridge_rpi_hw PUBLIC
        pifridge_light_logic
        Threads::Threads
        ${GPIOD_LIBRARIES}
    )

    target_compile_options(pifridge_rpi_hw PUBLIC
        ${GPIOD_CFLAGS_OTHER}
    )

    if(PIFRIDGE_HAVE_GPIOD_V2)
        target_compile_definitions(pifridge_rpi_hw PUBLIC PIFRIDGE_GPIOD_V2=1)
    else()
        target_compile_definitions(pifridge_rpi_hw PUBLIC PIFRIDGE_GPIOD_V2=0)
    endif()

    add_executable(pifridge_light_sensor_demo
        main.cpp
    )

    target_link_libraries(pifridge_light_sensor_demo PRIVATE
        pifridge_rpi_hw
    )
endif()
