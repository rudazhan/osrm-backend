# The target includes object files compiled from source files (and headers).
# Reference the objects as $<TARGET_OBJECTS:STORAGE>
#file(GLOB StorageGlob src/storage/*.cpp)
#add_library(STORAGE OBJECT ${StorageGlob})

set(EXTERNAL_LIBRARIES
        ${BOOST_BASE_LIBRARIES}
        ${Boost_PROGRAM_OPTIONS_LIBRARY}
        ${CMAKE_THREAD_LIBS_INIT}
        ${TBB_LIBRARIES}
        ${MAYBE_RT_LIBRARY}
        ${MAYBE_COVERAGE_LIBRARIES})

add_executable(route-distribution RouteDistribution.cpp $<TARGET_OBJECTS:UTIL> ../../include/util/csv_reader.hpp)
target_link_libraries(route-distribution osrm ${EXTERNAL_LIBRARIES})
