add_subdirectory(common)   
add_subdirectory(BME680)
add_subdirectory(BH1750)
add_subdirectory(web_app)
add_subdirectory(BarcodeScanner)
add_subdirectory(Camera)
find_library(SQLITE_LIB sqlite3 REQUIRED)

# -- PiFridge main executable --
add_executable(pifridge main.cpp)
 
target_include_directories(pifridge
    PRIVATE ${CMAKE_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/common
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/BME680
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/BH1750/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/BarcodeScanner
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Camera
)
 
target_link_libraries(pifridge
    PRIVATE linux_i2c
    PRIVATE bme680
    PRIVATE bh1750
    PRIVATE barcode_scanner
    PRIVATE camera
    PRIVATE Threads::Threads
    PRIVATE CURL::libcurl
    PRIVATE ${SQLITE_LIB}
)
 