

set (CMAKE_CXX_STANDARD 20)

message(STATUS "Configuring examples")

file(GLOB SOURCES
echoserver_coro.cpp
echoserver_movable.cpp
echoserver.cpp
echoclient.cpp
http/beast_server.cpp
http/beast_client.cpp
)

file(GLOB HEADERS
*.hpp
)

# set(SOURCES echoserver.cpp)

foreach(exe_file ${SOURCES})
    get_filename_component(exe_name ${exe_file} NAME_WE)
    add_executable(${exe_name} ${exe_file} ${HEADERS})
    target_link_libraries(${exe_name} PRIVATE winasio)
endforeach()