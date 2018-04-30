cmake_minimum_required(VERSION 3.10)
project(static_route C)

set(CMAKE_C_STANDARD 99)

add_executable(router router.c router.c arp.c net.c)