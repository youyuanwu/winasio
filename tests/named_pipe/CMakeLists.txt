file(GLOB SOURCES
*_test.cpp
)

# strip file extension
foreach(test_file ${SOURCES})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_executable(${test_name} ${test_file})
    target_include_directories(${test_name} 
      PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../examples
      PRIVATE .
    )
    target_link_libraries(${test_name} PRIVATE Boost::ut winasio Boost::log spdlog::spdlog)
    set_property(TARGET ${test_name} PROPERTY CXX_STANDARD 20) # for semaphore
    add_test(NAME ${test_name} COMMAND ${test_name} WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endforeach()