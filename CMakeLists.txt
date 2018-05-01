cmake_minimum_required(VERSION 3.10)
project(static_route C)

set(CMAKE_C_STANDARD 99)

find_package(Threads)

add_executable(router ip.c arp.c inet.c icmp.c interface.c router.c)
target_link_libraries(router ${CMAKE_THREAD_LIBS_INIT})