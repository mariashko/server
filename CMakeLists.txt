cmake_minimum_required(VERSION 2.8.4)
project(tcp_server)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread")

include_directories(src)
set(SOURCE_FILES
                    src/tcp/tcp_socket.h
                    src/tcp/tcp_socket.cpp
                    src/tcp/tcp_server.h
                    src/tcp/tcp_server.cpp
                    src/async_service/epoll.h
                    src/async_service/epoll.cpp
                    src/async_service/io_service.h
                    src/async_service/io_service.cpp
                    src/main.cpp)

add_executable(tcp_server ${SOURCE_FILES})