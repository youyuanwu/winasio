cmake_minimum_required(VERSION 3.14)

project(winasio)

set(winasio_MAIN_PROJECT OFF)
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(winasio_MAIN_PROJECT ON)
endif()

option(winasio_BuildTests     "Build the unit tests when BUILD_TESTING is enabled." ${winasio_MAIN_PROJECT})
option(winasio_BuildExamples  "Build examples"                                      ${winasio_MAIN_PROJECT})

find_package(Boost REQUIRED)

add_library(winasio INTERFACE)
target_include_directories(winasio INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Boost_INCLUDE_DIR}
)

# header only
target_compile_definitions(winasio
    INTERFACE BOOST_ALL_NO_LIB=1
    INTERFACE _WIN32_WINNT=0x0601
)

if(winasio_BuildExamples)
    add_subdirectory(examples)
endif()

if(winasio_BuildTests)
    enable_testing()
    add_subdirectory(tests)
endif()