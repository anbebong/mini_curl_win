// Mini Curl - C API for DLL (exported functions)
//
// Header này định nghĩa C API để sử dụng mini_curl DLL trực tiếp
// Không cần wrapper, có thể load bằng LoadLibrary/GetProcAddress

#ifndef MINI_CURL_C_H
#define MINI_CURL_C_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

// Handle cho HTTP client
typedef void* MiniCurlHandle;

// Response structure
typedef struct {
    int statusCode;
    char* body;           // Phải gọi mini_curl_response_free_body để giải phóng
    size_t bodySize;
    char* error;          // Phải gọi mini_curl_response_free_error để giải phóng
} MiniCurlResponse;

// Options structure
typedef struct {
    const char* userAgent;
    int timeout;
    int followRedirects;
    int verifySSL;
    const char* proxy;
    const char* proxyUser;
    const char* proxyPassword;
    const char* username;
    const char* password;
    // Custom headers (format: "Header1: Value1\nHeader2: Value2" hoặc "Header1: Value1\r\nHeader2: Value2")
    // Ví dụ: "X-Auth-Token: abc123\nAuthorization: Bearer xyz"
    const char* customHeaders;
} MiniCurlOptions;

// Exported functions (sẽ được export từ DLL)
// Sử dụng __declspec(dllexport) khi build DLL
// Sử dụng __declspec(dllimport) khi link với DLL

#ifdef MINI_CURL_C_EXPORTS
    #define MINI_CURL_C_API __declspec(dllexport)
#else
    #define MINI_CURL_C_API __declspec(dllimport)
#endif

// Tạo HTTP client
MINI_CURL_C_API MiniCurlHandle mini_curl_c_create(void);

// Giải phóng HTTP client
MINI_CURL_C_API void mini_curl_c_destroy(MiniCurlHandle handle);

// Khởi tạo HTTP client
MINI_CURL_C_API int mini_curl_c_init(MiniCurlHandle handle);

// Cleanup HTTP client
MINI_CURL_C_API void mini_curl_c_cleanup(MiniCurlHandle handle);

// GET request
MINI_CURL_C_API MiniCurlResponse* mini_curl_c_get(MiniCurlHandle handle, 
                                                   const char* url, 
                                                   const MiniCurlOptions* options);

// POST request
MINI_CURL_C_API MiniCurlResponse* mini_curl_c_post(MiniCurlHandle handle,
                                                     const char* url,
                                                     const char* data,
                                                     const char* contentType,
                                                     const MiniCurlOptions* options);

// PUT request
MINI_CURL_C_API MiniCurlResponse* mini_curl_c_put(MiniCurlHandle handle,
                                                   const char* url,
                                                   const char* data,
                                                   const char* contentType,
                                                   const MiniCurlOptions* options);

// DELETE request
MINI_CURL_C_API MiniCurlResponse* mini_curl_c_delete(MiniCurlHandle handle,
                                                       const char* url,
                                                       const MiniCurlOptions* options);

// PATCH request
MINI_CURL_C_API MiniCurlResponse* mini_curl_c_patch(MiniCurlHandle handle,
                                                      const char* url,
                                                      const char* data,
                                                      const char* contentType,
                                                      const MiniCurlOptions* options);

// Generic request (có thể dùng cho bất kỳ HTTP method nào)
MINI_CURL_C_API MiniCurlResponse* mini_curl_c_request(MiniCurlHandle handle,
                                                        const char* method,
                                                        const char* url,
                                                        const char* data,
                                                        const char* contentType,
                                                        const MiniCurlOptions* options);

// Giải phóng response
MINI_CURL_C_API void mini_curl_c_response_free(MiniCurlResponse* response);

#ifdef __cplusplus
}
#endif

#endif // MINI_CURL_C_H

