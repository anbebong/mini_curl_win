cmake_minimum_required(VERSION 3.10)
project(mini_curl LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MinGW specific settings
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(STATUS "Using MinGW-w64 compiler")
    # Set MinGW paths if needed
    if(NOT DEFINED ENV{MINGW_PREFIX})
        # Try to detect MinGW installation
        get_filename_component(MINGW_PATH ${CMAKE_CXX_COMPILER} DIRECTORY)
        get_filename_component(MINGW_PATH ${MINGW_PATH} DIRECTORY)
        if(EXISTS "${MINGW_PATH}")
            message(STATUS "Detected MinGW installation: ${MINGW_PATH}")
        endif()
    endif()
endif()

# Source files
set(SOURCES
    src/winhttp_curl.cpp
)

set(HEADERS
    include/winhttp_curl.h
)

# Option to build as DLL or static library
# Mặc định build static library (không cần DLL) - tất cả code được link trực tiếp vào executable
option(BUILD_SHARED_LIBS "Build shared library (DLL)" OFF)

# Create library (STATIC or SHARED based on BUILD_SHARED_LIBS)
# Mặc định: STATIC (static linking) - không cần DLL, tất cả code được compile vào executable
if(BUILD_SHARED_LIBS)
    # Build as DLL (shared library) - cần DLL file khi chạy
    add_library(mini_curl SHARED ${SOURCES} ${HEADERS})
    target_compile_definitions(mini_curl PRIVATE BUILD_DLL MINI_CURL_EXPORTS)
    # Set DLL output name
    set_target_properties(mini_curl PROPERTIES
        OUTPUT_NAME "mini_curl"
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    )
    message(STATUS "Building mini_curl as SHARED library (DLL)")
else()
    # Build as STATIC library - code được link trực tiếp vào executable
    add_library(mini_curl STATIC ${SOURCES} ${HEADERS})
    target_compile_definitions(mini_curl PUBLIC MINI_CURL_STATIC)
    message(STATUS "Building mini_curl as STATIC library (static linking)")
endif()

# Set include directories for library (public headers)
# Sử dụng CMAKE_CURRENT_SOURCE_DIR để đảm bảo path đúng khi build từ parent
target_include_directories(mini_curl 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Link WinHTTP library
target_link_libraries(mini_curl winhttp)

# Install rules
install(TARGETS mini_curl
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES ${HEADERS} DESTINATION include)

# Example executable - API Client (C)
add_executable(example_api_simple_c 
    examples/example_api_simple.c 
    examples/api_client_c.cpp
)
target_include_directories(example_api_simple_c PRIVATE 
    $<TARGET_PROPERTY:mini_curl,INTERFACE_INCLUDE_DIRECTORIES>
    examples
)
target_link_libraries(example_api_simple_c mini_curl winhttp)

# Define MINI_CURL_STATIC for static linking (already defined in mini_curl target)

# Set output directory
set_target_properties(example_api_simple_c PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

