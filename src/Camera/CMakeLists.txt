find_package(OpenCV REQUIRED)

set(CAMERA_SOURCES
    Camera.cpp
)

add_library(camera STATIC ${CAMERA_SOURCES})

target_include_directories(camera
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(camera
    PUBLIC ${OpenCV_LIBS}
    PUBLIC tensorflow-lite
)