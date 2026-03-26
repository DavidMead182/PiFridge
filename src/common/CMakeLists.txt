add_library(linux_i2c
    LinuxI2CDevice.cpp
)
 
target_include_directories(linux_i2c
    PUBLIC  ${CMAKE_SOURCE_DIR}/include        # II2CDevice.hpp
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}        
)
 
 
