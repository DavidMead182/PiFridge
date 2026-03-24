add_subdirectory(common)   # linux_i2c built first - sensors depend on it
add_subdirectory(BME680)
add_subdirectory(BH1750)
 
# -- PiFridge main executable --
add_executable(pifridge main.cpp)
 
target_include_directories(pifridge
    PRIVATE ${CMAKE_SOURCE_DIR}/include          # II2CDevice.hpp
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/BME680   # BME680Sensor.hpp
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/common   # LinuxI2CDevice.hpp
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/BH1750   # Bh1750Sensor.hpp etc
)
 
target_link_libraries(pifridge
    PRIVATE bme680
    PRIVATE linux_i2c
    PRIVATE pifridge_rpi_hw      # BH1750, GpioOutput, DoorLightController
    PRIVATE Threads::Threads
)
 