add_subdirectory(common)   # linux_i2c built first - sensors depend on it
add_subdirectory(BME680)

add_executable(pifridge main.cpp)
 
target_include_directories(pifridge
    PRIVATE ${CMAKE_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/BME680
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/common
)
 
target_link_libraries(pifridge
    PRIVATE bme680
    PRIVATE linux_i2c
    PRIVATE Threads::Threads
)