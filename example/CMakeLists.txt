add_executable(osrm-example example.cpp)
target_link_libraries(osrm-example ${LibOSRM_LIBRARIES} ${LibOSRM_DEPENDENT_LIBRARIES})

target_include_directories(osrm-example PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LibOSRM_CXXFLAGS}")
