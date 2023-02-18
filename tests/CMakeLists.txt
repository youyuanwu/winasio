message(STATUS "Configuring tests")

set(Boost_USE_STATIC_LIBS ON) # use static boost
find_package(Boost REQUIRED COMPONENTS unit_test_framework )

add_subdirectory(http)
add_subdirectory(winhttp)
add_subdirectory(named_pipe)