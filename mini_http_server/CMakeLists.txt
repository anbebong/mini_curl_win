cmake_minimum_required(VERSION 3.14)
project(mini_http_server LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MinGW specific settings
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(STATUS "Using MinGW-w64 compiler")
endif()

# Download Mongoose library từ GitHub (external dependency)
# Sử dụng file download thay vì git để tránh phụ thuộc vào git trong Docker
set(MONGOOSE_VERSION "7.20")
set(MONGOOSE_URL "https://github.com/cesanta/mongoose/archive/refs/tags/${MONGOOSE_VERSION}.tar.gz")
set(MONGOOSE_DOWNLOAD_DIR "${CMAKE_BINARY_DIR}/_deps/mongoose-download")
set(MONGOOSE_EXTRACT_DIR "${CMAKE_BINARY_DIR}/_deps/mongoose-${MONGOOSE_VERSION}")
set(MONGOOSE_SOURCE_DIR "${CMAKE_BINARY_DIR}/_deps/mongoose-src")

# Kiểm tra xem mongoose đã được download chưa
if(NOT EXISTS "${MONGOOSE_SOURCE_DIR}/mongoose.c")
    message(STATUS "Downloading Mongoose library from GitHub (version ${MONGOOSE_VERSION})...")
    
    # Tạo thư mục download
    file(MAKE_DIRECTORY "${MONGOOSE_DOWNLOAD_DIR}")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/_deps")
    
    # Download file tar.gz
    set(MONGOOSE_ARCHIVE "${MONGOOSE_DOWNLOAD_DIR}/mongoose-${MONGOOSE_VERSION}.tar.gz")
    
    # Sử dụng file(DOWNLOAD) thay vì git
    file(DOWNLOAD 
        "${MONGOOSE_URL}"
        "${MONGOOSE_ARCHIVE}"
        TIMEOUT 60
        STATUS DOWNLOAD_STATUS
        SHOW_PROGRESS
    )
    
    # Kiểm tra kết quả download
    list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
    if(NOT STATUS_CODE EQUAL 0)
        list(GET DOWNLOAD_STATUS 1 ERROR_MESSAGE)
        message(FATAL_ERROR "Failed to download mongoose: ${ERROR_MESSAGE}")
    endif()
    
    # Extract tar.gz
    message(STATUS "Extracting Mongoose library...")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xzf "${MONGOOSE_ARCHIVE}"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/_deps"
        RESULT_VARIABLE EXTRACT_RESULT
        OUTPUT_QUIET
        ERROR_QUIET
    )
    
    if(NOT EXTRACT_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to extract mongoose archive")
    endif()
    
    # Kiểm tra và rename extracted directory
    if(EXISTS "${MONGOOSE_EXTRACT_DIR}/mongoose.c")
        # Directory đã được extract đúng
        if(EXISTS "${MONGOOSE_SOURCE_DIR}")
            file(REMOVE_RECURSE "${MONGOOSE_SOURCE_DIR}")
        endif()
        file(RENAME 
            "${MONGOOSE_EXTRACT_DIR}"
            "${MONGOOSE_SOURCE_DIR}"
        )
    else()
        message(FATAL_ERROR "Mongoose extraction failed: mongoose.c not found in ${MONGOOSE_EXTRACT_DIR}")
    endif()
    
    message(STATUS "Mongoose downloaded and extracted to: ${MONGOOSE_SOURCE_DIR}")
else()
    message(STATUS "Mongoose already available at: ${MONGOOSE_SOURCE_DIR}")
endif()

# Đảm bảo MONGOOSE_SOURCE_DIR được set đúng
if(NOT EXISTS "${MONGOOSE_SOURCE_DIR}/mongoose.c")
    message(FATAL_ERROR "Mongoose source not found at: ${MONGOOSE_SOURCE_DIR}/mongoose.c")
endif()

# Option to build as DLL or static library
option(BUILD_SHARED_LIBS "Build shared library (DLL)" OFF)

# Mini HTTP Server Library
if(BUILD_SHARED_LIBS)
    add_library(mini_http_server SHARED 
        src/oidc_config.cpp
        src/oidc_token_exchange.cpp
        src/mini_http_server_c.cpp
        ${MONGOOSE_SOURCE_DIR}/mongoose.c
    )
    target_compile_definitions(mini_http_server PRIVATE BUILD_DLL MINI_HTTP_SERVER_EXPORTS)
    target_compile_definitions(mini_http_server PUBLIC MINI_HTTP_SERVER_EXPORTS)
    message(STATUS "Building mini_http_server as SHARED library (DLL)")
else()
    add_library(mini_http_server STATIC 
        src/oidc_config.cpp
        src/oidc_token_exchange.cpp
        src/mini_http_server_c.cpp
        ${MONGOOSE_SOURCE_DIR}/mongoose.c
    )
    target_compile_definitions(mini_http_server PUBLIC MINI_HTTP_SERVER_STATIC)
    message(STATUS "Building mini_http_server as STATIC library")
endif()

target_include_directories(mini_http_server PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_include_directories(mini_http_server PRIVATE 
    ${MONGOOSE_SOURCE_DIR}
    src
)

# Link HTTP client library - phân biệt Windows và Linux
if(WIN32)
    # Windows: Sử dụng mini_curl (WinHTTP)
    if(TARGET mini_curl)
        target_link_libraries(mini_http_server mini_curl)
        target_compile_definitions(mini_http_server PRIVATE USE_MINI_CURL)
    else()
        message(FATAL_ERROR "mini_curl library not found. Please build from root CMakeLists.txt or add mini_curl as subdirectory")
    endif()
    # Link Winsock và WinHTTP libraries cho Windows
    target_link_libraries(mini_http_server ws2_32 winhttp)
else()
    # Linux: Chỉ sử dụng libcurl từ external (không dùng system)
    # Tìm libcurl từ external/curl (static)
    set(CURL_LIB_DIR "${CMAKE_SOURCE_DIR}/../curl/lib/.libs")
    set(CURL_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/../curl/include")
    
    # Kiểm tra libcurl.a có tồn tại không
    if(EXISTS "${CURL_LIB_DIR}/libcurl.a")
        target_link_libraries(mini_http_server "${CURL_LIB_DIR}/libcurl.a")
        target_include_directories(mini_http_server PRIVATE "${CURL_INCLUDE_DIR}")
        target_compile_definitions(mini_http_server PRIVATE USE_LIBCURL)
        message(STATUS "Using static libcurl from: ${CURL_LIB_DIR}/libcurl.a")
        
        # Link với OpenSSL và zlib (nếu có)
        if(EXISTS "${CMAKE_SOURCE_DIR}/../openssl/libssl.a")
            target_link_libraries(mini_http_server 
                "${CMAKE_SOURCE_DIR}/../openssl/libssl.a"
                "${CMAKE_SOURCE_DIR}/../openssl/libcrypto.a"
            )
            target_include_directories(mini_http_server PRIVATE "${CMAKE_SOURCE_DIR}/../../openssl/include")
        endif()
        
        if(EXISTS "${CMAKE_SOURCE_DIR}/../zlib/libz.a")
            target_link_libraries(mini_http_server "${CMAKE_SOURCE_DIR}/../zlib/libz.a")
            target_include_directories(mini_http_server PRIVATE "${CMAKE_SOURCE_DIR}/../zlib")
        endif()
    else()
        message(FATAL_ERROR "libcurl.a not found in ${CURL_LIB_DIR}. Please build curl in external/curl first.")
    endif()
    
    # Link với pthread và các thư viện hệ thống cần thiết cho Linux
    target_link_libraries(mini_http_server pthread dl)
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

