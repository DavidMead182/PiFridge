add_library(bh1750 STATIC
    ILightSensor.cpp
    DoorLightController.cpp
    Bh1750Sensor.cpp
)

target_include_directories(bh1750 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

