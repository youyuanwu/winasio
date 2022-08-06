cmake_minimum_required(VERSION 3.14)

project(winext)

set(WINEXT_MAIN_PROJECT OFF)
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(WINEXT_MAIN_PROJECT ON)
endif()

option(WINEXT_BuildTests     "Build the unit tests when BUILD_TESTING is enabled." ${WINEXT_MAIN_PROJECT})
option(WINEXT_BuildExamples  "Build examples"                                      ${WINEXT_MAIN_PROJECT})

find_package(Boost REQUIRED)

add_library(winext INTERFACE)
target_include_directories(winext INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Boost_INCLUDE_DIR}
)

# header only
target_compile_definitions(winext
    INTERFACE BOOST_ALL_NO_LIB=1
    INTERFACE _WIN32_WINNT=0x0601
)

if(WINEXT_BuildExamples)
    add_subdirectory(examples)
endif()

if(WINEXT_BuildTests)
    enable_testing()
    add_subdirectory(tests)
endif()