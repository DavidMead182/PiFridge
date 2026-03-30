# Create BME680 library
add_library(bme680
    BME680.cpp
    BME680Sensor.cpp
)

# Include directories for BME680
target_include_directories(bme680
    PUBLIC  ${CMAKE_SOURCE_DIR}/include       # II2CDevice.hpp
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}       # BME680.hpp
    PRIVATE ${CMAKE_SOURCE_DIR}/src/common    # LinuxI2CDevice.hpp
)

# Link BME680 with linux_i2c 
target_link_libraries(bme680
    PRIVATE linux_i2c
)

# Optional demo executable
add_executable(bme680_demo demo_main.cpp)

target_include_directories(bme680_demo
    PRIVATE ${CMAKE_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(bme680_demo
    PRIVATE bme680
    PRIVATE linux_i2c
)