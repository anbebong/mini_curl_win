cmake_minimum_required(VERSION 3.10)
project(call-api LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Option to build as DLL or static library
option(BUILD_SHARED_LIBS "Build shared library (DLL)" OFF)

# Build mini_curl first
add_subdirectory(mini_curl)

# Build mini_http_server (depends on mini_curl)
add_subdirectory(mini_http_server)
