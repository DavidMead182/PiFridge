find_library(SQLITE_LIB sqlite3 REQUIRED)

# Sources
set(BARCODESCANNER_SOURCES
    BarcodeScanner.cpp
)

# Create a static library
add_library(barcode_scanner STATIC ${BARCODESCANNER_SOURCES})

target_include_directories(barcode_scanner
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} 
)

target_link_libraries(barcode_scanner
    PUBLIC ${SQLITE_LIB}
    PUBLIC curl
)

add_executable(barcode_scanner_demo
    test/BarcodeScannerDemo.cpp
)

target_link_libraries(barcode_scanner_demo
    PRIVATE barcode_scanner
    PUBLIC ${SQLITE_LIB}
    PUBLIC curl
)

target_include_directories(barcode_scanner_demo
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} 
)