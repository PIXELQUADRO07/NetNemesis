cmake_minimum_required(VERSION 3.10)
project(NetNemesis VERSION 4.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -Wextra")

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "NetNemesis richiede Linux")
endif()

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)  # Per crittografia

add_executable(netnemesis
    src/main.cpp
    src/packet_crafter.cpp
    src/botnet.cpp
    src/attacks.cpp
    src/scanner.cpp
    src/utils.cpp
    src/fingerprint.cpp      # Nuovo
    src/crypto.cpp           # Nuovo
    src/icmp_flood.cpp       # Nuovo
    src/arp_spoofer.cpp      # Nuovo
    src/slowloris.cpp        # Nuovo
)

target_include_directories(netnemesis PRIVATE include)
target_link_libraries(netnemesis PRIVATE 
    Threads::Threads
    OpenSSL::Crypto          # Link OpenSSL
    OpenSSL::SSL
)