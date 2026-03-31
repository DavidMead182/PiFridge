find_library(FCGI_LIB   fcgi    REQUIRED)
find_library(SQLITE_LIB sqlite3 REQUIRED)
 

add_executable(pifridge_api pifridge_api.cpp)
target_link_libraries(pifridge_api
    PRIVATE ${FCGI_LIB}
)
 
add_executable(pifridge_inventory pifridge_inventory.cpp)
target_link_libraries(pifridge_inventory
    PRIVATE ${FCGI_LIB}
    PRIVATE ${SQLITE_LIB}
)
 