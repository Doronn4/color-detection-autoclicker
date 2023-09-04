cmake_minimum_required(VERSION 3.0.0)
project(coca VERSION 0.1.0)

find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

find_package(X11 REQUIRED)
include_directories(${X11_INCLUDE_DIRS})

# Set the path to the XTest library
set(XTEST_LIBRARY "/usr/lib64/libXtst.so") # Replace with the actual path

if (NOT EXISTS ${XTEST_LIBRARY})
    message(FATAL_ERROR "XTest library not found.")
endif()

add_executable(cocacola main.cpp)

target_link_libraries(cocacola PRIVATE ${OpenCV_LIBS})
target_link_libraries(cocacola PRIVATE ${X11_LIBRARIES})
target_link_libraries(cocacola PRIVATE ${XTEST_LIBRARY}) # Add XTest to the target's libraries

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
