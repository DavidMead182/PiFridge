# Sources
set(BARCODESCANNER_SOURCES
    BarcodeScanner.cpp
)

# Create a static library
add_library(barcode_scanner STATIC ${BARCODESCANNER_SOURCES})


target_include_directories(barcode_scanner
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} 
)