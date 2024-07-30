cmake_minimum_required(VERSION 3.7)

project(easy_net)
set(CMAKE_C_STANDARD 99)

LINK_DIRECTORIES(
        lib/npcap/Lib/x64
)

include_directories(
        ${PROJECT_SOURCE_DIR}
        lib/npcap/Include
        ${PROJECT_SOURCE_DIR}/include/stack
        ${PROJECT_SOURCE_DIR}/include/common
        ${PROJECT_SOURCE_DIR}/test
        ${PROJECT_SOURCE_DIR}/include/platform
        ${PROJECT_SOURCE_DIR}/include/api
        ${PROJECT_SOURCE_DIR}/include/stack/link
        ${PROJECT_SOURCE_DIR}/include/stack/network
        ${PROJECT_SOURCE_DIR}/include/stack/transport
)

file(GLOB_RECURSE SOURCE_LIST "src/*.c" "src/*.h" "test/*.c" "test/*.h" "include/*.h")

add_executable(${PROJECT_NAME} ${SOURCE_LIST}
        src/stack/app/echo/udp_echo_client.h
        src/stack/app/echo/udp_echo_client.c
        src/stack/app/echo/udp_echo_server.h
        src/stack/app/echo/udp_echo_server.c
        include/stack/transport/udp.h
        src/stack/transport/udp.c
        src/stack/transport/tcp.c
        include/stack/transport/tcp.h
        src/stack/app/echo/tcp_echo_client.h
        src/stack/app/echo/tcp_echo_client.c
        include/stack/transport/tcp_in.h
        src/stack/transport/tcp_in.c
        include/stack/transport/tcp_out.h
        src/stack/transport/tcp_out.c
        src/stack/transport/tcp_state.c
        include/stack/transport/tcp_state.h
        include/stack/transport/tcp_buf.h
        src/stack/transport/tcp_buf.c
        test/download_test.c
        src/stack/app/echo/tcp_echo_server.h
        src/stack/app/echo/tcp_echo_server.c)

add_definitions(-DNET_DRIVER_PCAP)
add_definitions(-D_CRT_SECURE_NO_WARNINGS)



message(STATUS "current platform: ${CMAKE_HOST_SYSTEM_NAME}")
if(CMAKE_HOST_SYSTEM_NAME MATCHES "Windows")
    add_definitions(-DSYS_PLAT_WINDOWS)
    target_link_libraries(${PROJECT_NAME} wpcap packet Ws2_32)
else()
    add_definitions(-DSYS_PLAT_LINUX)
    target_link_libraries(${PROJECT_NAME} pthread pcap)
endif()