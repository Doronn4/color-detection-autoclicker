# CMakeList.txt : CMake project for autoclicker, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("autoclicker")

set(CMAKE_TOOLCHAIN_FILE "C:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")

# Add source to this project's executable.
add_executable (autoclicker "main.cpp")

set_target_properties(autoclicker PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY "G:/projects/coca/simulator/bin"
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET autoclicker PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.

find_package(OpenCV REQUIRED)
find_package(Boost REQUIRED COMPONENTS program_options)

include_directories(${OpenCV_INCLUDE_DIRS} ${Boost_INCLUDE_DIR})

target_link_libraries(autoclicker PRIVATE ${OpenCV_LIBS} PRIVATE Boost::program_options)
