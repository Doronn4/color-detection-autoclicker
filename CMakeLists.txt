cmake_minimum_required(VERSION 3.0.0)
project(coca VERSION 0.1.0)

# include(CTest)
# enable_testing()

find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

# Transform dynamic library paths to static library paths

add_executable(cocacola main.cpp)

target_link_libraries(cocacola PRIVATE ${OpenCV_LIBS})

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
