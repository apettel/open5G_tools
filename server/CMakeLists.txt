cmake_minimum_required(VERSION 3.10)
project(open5G_server C)

set(CMAKE_C_STANDARD 11)

add_executable(server server.c)
set_target_properties(server PROPERTIES OUTPUT_NAME "open5G_server")
