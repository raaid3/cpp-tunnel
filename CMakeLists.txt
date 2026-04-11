cmake_minimum_required(VERSION 3.10)

project(tun_project)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(tun src/main.cpp src/tun.cpp)