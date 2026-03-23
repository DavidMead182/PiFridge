add_library(bme680
    BME680.cpp
)
 
target_include_directories(bme680
    PUBLIC  ${CMAKE_SOURCE_DIR}/include        # II2CDevice.hpp
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}        # BME680.hpp
)
 
add_executable(bme680_demo demo_main.cpp)
 
target_include_directories(bme680_demo
    PRIVATE ${CMAKE_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_SOURCE_DIR}/src/common     # LinuxI2CDevice.hpp
)
 
target_link_libraries(bme680_demo
    PRIVATE bme680
    PRIVATE linux_i2c
    PRIVATE pthread
)
 