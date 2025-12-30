cmake_minimum_required(VERSION 3.14)

set(winasio_MAIN_PROJECT OFF)
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(winasio_MAIN_PROJECT ON)
endif()

if(winasio_MAIN_PROJECT AND NOT WINASIO_CI_FORMAT)
    # configure vcpkg
    # we use CmakePresets.json to point to vcpkg
    if ("$ENV{VCPKG_ROOT}" STREQUAL "")
    message(FATAL_ERROR "VCPKG_ROOT not found")
    endif()
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
endif()

project(winasio VERSION 0.0.1 LANGUAGES CXX)

message(STATUS "vcpkg installed dir: ${VCPKG_INSTALLED_DIR}")

option(winasio_BuildTests     "Build the unit tests when BUILD_TESTING is enabled." ${winasio_MAIN_PROJECT})
option(winasio_BuildExamples  "Build examples"                                      ${winasio_MAIN_PROJECT})

# format
if(${winasio_MAIN_PROJECT})
include(cmake/clang-format.cmake)
if(WINASIO_CI_FORMAT)
    message(STATUS "Only added format target.")
    return()
endif()
endif(${winasio_MAIN_PROJECT})


set(Boost_USE_STATIC_LIBS ON) # use static boost
set(Boost_NO_WARN_NEW_VERSIONS ON)
find_package(Boost REQUIRED COMPONENTS log unit_test_framework headers)
find_package(spdlog CONFIG REQUIRED)
find_package(ut CONFIG REQUIRED)

file(GLOB_RECURSE WINASIO_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp)

add_library(winasio INTERFACE ${WINASIO_SOURCES})
target_include_directories(winasio INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# header only
target_compile_definitions(winasio
    INTERFACE _WIN32_WINNT=0x0602
#    INTERFACE WINASIO_LOG=1
)

# good practice
target_compile_options(winasio
  INTERFACE /W4 /WX
)

# currently winasio uses boost log for logging
target_link_libraries(winasio
    INTERFACE Boost::headers
    INTERFACE spdlog::spdlog
)

if(winasio_BuildExamples)
    add_subdirectory(examples)
endif()

if(winasio_BuildTests)
    enable_testing()
    add_subdirectory(tests)

    # coverage
    find_program(OpenCppCoverage_exe
        NAMES OpenCppCoverage.exe
    )
    if(OpenCppCoverage_exe)
        message(STATUS "coverage tool found: ${OpenCppCoverage_exe}")
        # coverage tool only recognizes windows style path, backslash.
        file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" PWD_WIN_PATH)
        add_custom_target(coverage
            COMMAND ${OpenCppCoverage_exe} --quiet --export_type cobertura:cobertura.xml --cover_children 
            --sources "${PWD_WIN_PATH}\\src" --sources "${PWD_WIN_PATH}\\include" --modules "${PWD_WIN_PATH}"
            -- ctest -C Debug --test-dir build --repeat until-pass:3 --timeout 30 --output-on-failure
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )
    else()
        message(STATUS "coverage tool not found: ${OpenCppCoverage_exe}")
    endif()
endif()