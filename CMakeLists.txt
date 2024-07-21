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
)

file(GLOB_RECURSE SOURCE_LIST "src/*.c" "src/*.h" "test/*.c" "test/*.h" "include/*.h")

add_executable(${PROJECT_NAME} ${SOURCE_LIST})

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