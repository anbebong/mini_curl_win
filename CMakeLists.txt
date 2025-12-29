cmake_minimum_required(VERSION 3.10)
project(call-api LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Detect cross-compilation for Windows (MinGW)
if(CMAKE_CROSSCOMPILING)
    message(STATUS "Cross-compiling for Windows")
elseif(WIN32)
    message(STATUS "Building natively on Windows")
else()
    message(STATUS "Building on Linux/Unix")
endif()

# Option to build as DLL or static library
option(BUILD_SHARED_LIBS "Build shared library (DLL)" OFF)

# Build mini_curl first (chỉ trên Windows hoặc khi cross-compile cho Windows)
# Kiểm tra nếu đang cross-compile cho Windows hoặc đang build trên Windows
if(WIN32 OR (CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_NAME STREQUAL "Windows"))
    add_subdirectory(mini_curl)
endif()

# Build mini_http_server
# - Windows: phụ thuộc vào mini_curl
# - Linux: phụ thuộc vào libcurl (từ external hoặc system)
add_subdirectory(mini_http_server)

# Example: Curl DLL - Direct loading (LoadLibrary/GetProcAddress)
# Chỉ build trên Windows (cần mini_curl và LoadLibrary)
if(WIN32)
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
endif()

# Full Application Example - Kết hợp cả 2 libraries
# Cả 2 libraries đều link static
add_executable(example_full_app 
    examples/example_full_app.c
)

target_include_directories(example_full_app PRIVATE 
    $<TARGET_PROPERTY:mini_http_server,INTERFACE_INCLUDE_DIRECTORIES>
)
# Chỉ thêm mini_curl include trên Windows
if(WIN32)
    if(TARGET mini_curl)
        target_include_directories(example_full_app PRIVATE 
            $<TARGET_PROPERTY:mini_curl,INTERFACE_INCLUDE_DIRECTORIES>
        )
    endif()
endif()

# Link với mini_http_server
target_link_libraries(example_full_app mini_http_server)

# Link với mini_curl chỉ trên Windows
if(WIN32 OR (CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_NAME STREQUAL "Windows"))
    if(TARGET mini_curl)
        target_link_libraries(example_full_app mini_curl)
    endif()
    target_link_libraries(example_full_app ws2_32 winhttp)
else()
    # Linux: libcurl đã được link trong mini_http_server
    target_link_libraries(example_full_app pthread dl)
endif()

# Định nghĩa macro để dùng static library
target_compile_definitions(example_full_app PRIVATE MINI_CURL_STATIC)

set_target_properties(example_full_app PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
