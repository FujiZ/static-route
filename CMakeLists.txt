cmake_minimum_required(VERSION 3.10)
project(static_route C)

set(CMAKE_C_STANDARD 99)

find_package(Threads REQUIRED)

add_executable(router ip.c arp.c inet.c icmp.c interface.c router.c)
add_executable(host ip.c arp.c inet.c icmp.c interface.c host.c)

target_link_libraries(router Threads::Threads)
target_link_libraries(host Threads::Threads)
