add_subdirectory(common)   
add_subdirectory(BME680)
add_subdirectory(BH1750)
add_subdirectory(web_app)
 
add_subdirectory(BarcodeScanner)

# -- PiFridge main executable --
add_executable(pifridge main.cpp)
 
target_include_directories(pifridge
    PRIVATE ${CMAKE_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/common
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/BME680
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/BH1750
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/BarcodeScanner
)
 
target_link_libraries(pifridge
    PRIVATE linux_i2c
    PRIVATE bme680
    PRIVATE pifridge_rpi_hw      # BH1750, GpioOutput, DoorLightController
    PRIVATE barcode_scanner      
    PRIVATE Threads::Threads
)
 