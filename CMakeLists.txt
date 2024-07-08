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
)

file(GLOB_RECURSE SOURCE_LIST "src/*.c" "src/*.h" "test/*.c" "test/*.h" "include/*.h")

add_executable(${PROJECT_NAME} ${SOURCE_LIST}
        include/stack/msg_handler.h
        src/stack/msg_handler.c
        include/platform/net_plat.h
        src/platform/net_plat.c
        include/common/log.h
        src/common/log.c
        test/log_test.c
        test/testcase.h
        include/common/list.h
        src/common/list.c
        test/list_test.c)
#        src/stack/stack.c
#        include/stack/stack.h
#        include/constants/net_errors.h)

add_definitions(-DNET_STACK_DEBUG_LOG)
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