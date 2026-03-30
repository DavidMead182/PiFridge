find_library(FCGI_LIB fcgi REQUIRED)
 
add_executable(pifridge_api pifridge_api.cpp)
 
target_link_libraries(pifridge_api
    PRIVATE ${FCGI_LIB}
)
 