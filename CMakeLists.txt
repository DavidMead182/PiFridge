cmake_minimum_required(VERSION 3.16)

project(pi_fridge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


project(PiFridge VERSION 1.0.0 LANGUAGES CXX)
include(GNUInstallDirs)

enable_testing()

# Subdirectory containing the Barcode Scanner
add_subdirectory(${CMAKE_SOURCE_DIR}/src/barcodeScanner)

# Subdirectory containing BH1750 sensor
add_subdirectory(${CMAKE_SOURCE_DIR}/src/BH1750)

# Subdirectory containing BME680 sensor
add_subdirectory(${CMAKE_SOURCE_DIR}/src/BME680)

# Subdirectory containing object detection code
add_subdirectory(${CMAKE_SOURCE_DIR}/src/object_detect)

# Subdirectory containing Open Food Facts API integration
add_subdirectory(${CMAKE_SOURCE_DIR}/src/openfoodfactsAPI)

# Subdirectory containing web app code
add_subdirectory(${CMAKE_SOURCE_DIR}/src/web_app)

# Compile options to check the code
add_compile_options(-Wall -Wconversion -Wextra -pedantic)