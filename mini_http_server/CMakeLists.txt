cmake_minimum_required(VERSION 3.14)
project(mini_http_server LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MinGW specific settings
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(STATUS "Using MinGW-w64 compiler")
endif()

# Fetch Mongoose library từ GitHub (external dependency)
# Sẽ tự động tải về khi build, không cần có trong repo
include(FetchContent)

FetchContent_Declare(
    mongoose
    GIT_REPOSITORY https://github.com/cesanta/mongoose.git
    GIT_TAG        7.20  # Sử dụng version 7.12 (stable)
    GIT_SHALLOW    TRUE  # Chỉ clone shallow để tiết kiệm thời gian
)

# Tải mongoose về (nếu chưa có)
FetchContent_GetProperties(mongoose)
if(NOT mongoose_POPULATED)
    message(STATUS "Downloading Mongoose library from GitHub...")
    FetchContent_Populate(mongoose)
    message(STATUS "Mongoose downloaded to: ${mongoose_SOURCE_DIR}")
endif()

# Option to build as DLL or static library
option(BUILD_SHARED_LIBS "Build shared library (DLL)" OFF)

# Mini HTTP Server Library
if(BUILD_SHARED_LIBS)
    add_library(mini_http_server SHARED 
        src/oidc_config.cpp
        src/oidc_token_exchange.cpp
        src/mini_http_server_c.cpp
        ${mongoose_SOURCE_DIR}/mongoose.c
    )
    target_compile_definitions(mini_http_server PRIVATE BUILD_DLL MINI_HTTP_SERVER_EXPORTS)
    target_compile_definitions(mini_http_server PUBLIC MINI_HTTP_SERVER_EXPORTS)
    message(STATUS "Building mini_http_server as SHARED library (DLL)")
else()
    add_library(mini_http_server STATIC 
        src/oidc_config.cpp
        src/oidc_token_exchange.cpp
        src/mini_http_server_c.cpp
        ${mongoose_SOURCE_DIR}/mongoose.c
    )
    target_compile_definitions(mini_http_server PUBLIC MINI_HTTP_SERVER_STATIC)
    message(STATUS "Building mini_http_server as STATIC library")
endif()

target_include_directories(mini_http_server PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_include_directories(mini_http_server PRIVATE 
    ${mongoose_SOURCE_DIR}
    src
)

# Link với mini_curl library (từ parent project)
if(TARGET mini_curl)
    target_link_libraries(mini_http_server mini_curl)
else()
    message(FATAL_ERROR "mini_curl library not found. Please build from root CMakeLists.txt or add mini_curl as subdirectory")
endif()

# Link Winsock và WinHTTP libraries cho Windows
if(WIN32)
    target_link_libraries(mini_http_server ws2_32 winhttp)
endif()

# Install rules
install(TARGETS mini_http_server
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/mini_http_server.h DESTINATION include)

# Example: Simple HTTP Server (C)
add_executable(example_simple 
    examples/example_simple.c
)

target_include_directories(example_simple PRIVATE 
    $<TARGET_PROPERTY:mini_http_server,INTERFACE_INCLUDE_DIRECTORIES>
)

target_link_libraries(example_simple mini_http_server)

if(WIN32)
    target_link_libraries(example_simple ws2_32)
endif()

set_target_properties(example_simple PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Example: OIDC Callback Server (C)
add_executable(example_oidc 
    examples/example_oidc.c
)

target_include_directories(example_oidc PRIVATE 
    $<TARGET_PROPERTY:mini_http_server,INTERFACE_INCLUDE_DIRECTORIES>
)

target_link_libraries(example_oidc mini_http_server)

if(WIN32)
    target_link_libraries(example_oidc ws2_32)
endif()

set_target_properties(example_oidc PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

