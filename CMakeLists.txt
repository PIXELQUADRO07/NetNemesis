cmake_minimum_required(VERSION 3.10)
project(NetNemesis VERSION 3.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -Wextra")

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "NetNemesis richiede Linux per i raw sockets")
endif()

find_package(Threads REQUIRED)

add_executable(netnemesis
    src/main.cpp
    src/packet_crafter.cpp
    src/botnet.cpp
    src/attacks.cpp
    src/scanner.cpp
    src/utils.cpp
)

target_include_directories(netnemesis PRIVATE include)
target_link_libraries(netnemesis PRIVATE Threads::Threads)

install(TARGETS netnemesis DESTINATION bin)