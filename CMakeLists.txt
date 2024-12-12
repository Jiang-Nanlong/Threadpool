cmake_minimum_required(VERSION 3.30)
project(xcc)

set(CMAKE_CXX_STANDARD 20)

add_executable(xcc main.cpp
        threadpool.h
        threadpool.cpp)
