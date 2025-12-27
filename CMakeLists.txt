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

# Example: Curl DLL - Direct loading (LoadLibrary/GetProcAddress)
add_executable(example_curl_dll_direct
    examples/example_curl_dll_direct.c
)
target_include_directories(example_curl_dll_direct PRIVATE
    $<TARGET_PROPERTY:mini_curl,INTERFACE_INCLUDE_DIRECTORIES>
)
# Không link với mini_curl - sẽ load DLL động bằng LoadLibrary
set_target_properties(example_curl_dll_direct PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Full Application Example - Kết hợp cả 2 libraries
# Cả 2 libraries đều link static
add_executable(example_full_app 
    examples/example_full_app.c
)

target_include_directories(example_full_app PRIVATE 
    $<TARGET_PROPERTY:mini_http_server,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:mini_curl,INTERFACE_INCLUDE_DIRECTORIES>
)

# Link với cả 2 libraries (static)
target_link_libraries(example_full_app 
    mini_http_server
    mini_curl
)

if(WIN32)
    target_link_libraries(example_full_app ws2_32 winhttp)
endif()

# Định nghĩa macro để dùng static library
target_compile_definitions(example_full_app PRIVATE MINI_CURL_STATIC)

set_target_properties(example_full_app PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
