cmake_minimum_required(VERSION 3.7)

project(easy_net)
set(CMAKE_C_STANDARD 99)

LINK_DIRECTORIES(
        lib/npcap/Lib/x64
)

add_definitions(-D_CRT_SECURE_NO_WARNINGS)

include_directories(
        ${PROJECT_SOURCE_DIR}
        lib/npcap/Include
        ${PROJECT_SOURCE_DIR}/include
#        ${PROJECT_SOURCE_DIR}/src/platform_adaptor
#        ${PROJECT_SOURCE_DIR}/src/app
)

file(GLOB_RECURSE SOURCE_LIST "src/*.c" "src/*.h" "test/*.c")

add_executable(${PROJECT_NAME} ${SOURCE_LIST}
        src/stack/stack.c
        include/stack/stack.h)

add_definitions(-DNET_DRIVER_PCAP)

message(STATUS "current platform: ${CMAKE_HOST_SYSTEM_NAME}")
if(CMAKE_HOST_SYSTEM_NAME MATCHES "Windows")
    add_definitions(-DSYS_PLAT_WINDOWS)
    target_link_libraries(${PROJECT_NAME} wpcap packet Ws2_32)
else()
    # Linux和Mac上的特定配置
    add_definitions(-DSYS_PLAT_LINUX)
    target_link_libraries(${PROJECT_NAME} pthread pcap)
endif()