# Mini Curl - HTTP Client Library

Thư viện HTTP client đơn giản sử dụng WinHTTP (Windows), hỗ trợ cả C++ và C API.

## Tính năng

- ✅ HTTP methods: GET, POST, PUT, DELETE, PATCH
- ✅ HTTPS với SSL certificate verification (có thể tắt)
- ✅ Custom headers
- ✅ Timeout configuration
- ✅ Follow redirects
- ✅ Proxy support
- ✅ HTTP Basic Authentication
- ✅ Static library hoặc DLL
- ✅ C API để load DLL trực tiếp

## Cấu trúc

```
mini_curl/
├── include/
│   ├── winhttp_curl.h      # C++ API (class WinHttpCurl)
│   └── mini_curl_c.h       # C API (exported functions cho DLL)
├── src/
│   ├── winhttp_curl.cpp    # C++ implementation
│   └── mini_curl_c.cpp     # C API implementation
└── examples/
    ├── api_client.h         # C++ helper class
    ├── api_client_c.h       # C wrapper header
    └── api_client_c.cpp     # C wrapper implementation
```

## Build

### Build Static Library (Mặc định)

```bash
build_cmake.bat
# hoặc
cd build
cmake .. -G "MinGW Makefiles" -DBUILD_SHARED_LIBS=OFF
cmake --build .
```

**Kết quả:**
- `build/lib/libmini_curl.a` - Static library
- Executables đã link static (không cần DLL)

### Build DLL

```bash
build_cmake.bat dll
# hoặc
cd build
cmake .. -G "MinGW Makefiles" -DBUILD_SHARED_LIBS=ON
cmake --build .
```

**Kết quả:**
- `build/bin/libmini_curl.dll` - DLL file
- `build/lib/libmini_curl.dll.a` - Import library

## Sử dụng

### Cách 1: C++ API (Static Library)

```cpp
#include "winhttp_curl.h"

int main() {
    // Tạo và khởi tạo client
    WinHttpCurl http;
    if (!http.Init()) {
        printf("Failed to initialize\n");
        return 1;
    }
    
    // Cấu hình options
    HttpOptions options;
    options.verifySSL = true;   // Verify SSL certificate
    options.timeout = 30;        // 30 seconds timeout
    
    // Gọi GET request
    HttpResponse response = http.Get("https://api.example.com/data", options);
    
    if (response.statusCode == 200) {
        printf("Success: %s\n", response.body.c_str());
    } else {
        printf("Error: %s\n", response.error.c_str());
    }
    
    // Cleanup
    http.Cleanup();
    return 0;
}
```

### Cách 2: C API (Static Library)

```c
#include "mini_curl_c.h"

int main() {
    // Tạo client
    MiniCurlHandle curl = mini_curl_c_create();
    mini_curl_c_init(curl);
    
    // Cấu hình options
    MiniCurlOptions options = {0};
    options.verifySSL = 1;
    options.timeout = 30;
    
    // Gọi GET request
    MiniCurlResponse* response = mini_curl_c_get(curl, "https://api.example.com/data", &options);
    
    if (response && response->statusCode == 200) {
        printf("Success: %s\n", response->body);
    }
    
    // Giải phóng
    mini_curl_c_response_free(response);
    mini_curl_c_cleanup(curl);
    mini_curl_c_destroy(curl);
    return 0;
}
```

### Cách 3: Load DLL Trực Tiếp (Không cần link)

```c
#include <windows.h>
#include "mini_curl_c.h"

// Function pointer types
typedef MiniCurlHandle (*create_fn)(void);
typedef int (*init_fn)(MiniCurlHandle);
typedef MiniCurlResponse* (*get_fn)(MiniCurlHandle, const char*, const MiniCurlOptions*);
typedef void (*free_fn)(MiniCurlResponse*);
typedef void (*cleanup_fn)(MiniCurlHandle);
typedef void (*destroy_fn)(MiniCurlHandle);

int main() {
    // Load DLL
    HMODULE dll = LoadLibraryA("libmini_curl.dll");
    if (!dll) {
        printf("Failed to load DLL\n");
        return 1;
    }
    
    // Get function pointers
    create_fn create = (create_fn)GetProcAddress(dll, "mini_curl_c_create");
    init_fn init = (init_fn)GetProcAddress(dll, "mini_curl_c_init");
    get_fn get = (get_fn)GetProcAddress(dll, "mini_curl_c_get");
    free_fn free_response = (free_fn)GetProcAddress(dll, "mini_curl_c_response_free");
    cleanup_fn cleanup = (cleanup_fn)GetProcAddress(dll, "mini_curl_c_cleanup");
    destroy_fn destroy = (destroy_fn)GetProcAddress(dll, "mini_curl_c_destroy");
    
    // Sử dụng
    MiniCurlHandle curl = create();
    init(curl);
    
    MiniCurlOptions options = {0};
    options.verifySSL = 1;
    MiniCurlResponse* response = get(curl, "https://api.example.com/data", &options);
    
    if (response && response->statusCode == 200) {
        printf("Success: %s\n", response->body);
    }
    
    free_response(response);
    cleanup(curl);
    destroy(curl);
    FreeLibrary(dll);
    return 0;
}
```

Xem `examples/example_curl_dll_direct.c` để biết ví dụ đầy đủ.

## API Reference

### C++ API (winhttp_curl.h)

#### Class: WinHttpCurl

```cpp
class WinHttpCurl {
public:
    bool Init();                    // Khởi tạo HTTP session
    void Cleanup();                 // Cleanup HTTP session
    
    HttpResponse Get(const std::string& url, const HttpOptions& options = HttpOptions());
    HttpResponse Post(const std::string& url, const std::string& data, 
                     const HttpOptions& options = HttpOptions());
    HttpResponse Put(const std::string& url, const std::string& data,
                     const HttpOptions& options = HttpOptions());
    HttpResponse Delete(const std::string& url, const HttpOptions& options = HttpOptions());
    HttpResponse Patch(const std::string& url, const std::string& data,
                       const HttpOptions& options = HttpOptions());
};
```

#### Structure: HttpResponse

```cpp
struct HttpResponse {
    int statusCode;                                    // HTTP status code (200, 404, etc.)
    std::string body;                                  // Response body
    std::map<std::string, std::string> headers;        // Response headers
    std::string error;                                 // Error message (nếu có)
};
```

#### Structure: HttpOptions

```cpp
struct HttpOptions {
    std::map<std::string, std::string> headers;       // Custom headers
    std::string userAgent;                             // User-Agent header
    int timeout;                                       // Timeout in seconds
    bool followRedirects;                              // Follow redirects (301, 302, etc.)
    bool verifySSL;                                    // Verify SSL certificate
    std::string proxy;                                 // Proxy URL
    std::string proxyUser;                             // Proxy username
    std::string proxyPassword;                         // Proxy password
    std::string username;                              // HTTP Basic Auth username
    std::string password;                              // HTTP Basic Auth password
};
```

### C API (mini_curl_c.h)

#### Functions

```c
// Tạo và quản lý client
MiniCurlHandle mini_curl_c_create(void);
void mini_curl_c_destroy(MiniCurlHandle handle);
int mini_curl_c_init(MiniCurlHandle handle);
void mini_curl_c_cleanup(MiniCurlHandle handle);

// HTTP requests
MiniCurlResponse* mini_curl_c_get(MiniCurlHandle handle, 
                                   const char* url, 
                                   const MiniCurlOptions* options);
MiniCurlResponse* mini_curl_c_post(MiniCurlHandle handle,
                                     const char* url,
                                     const char* data,
                                     const char* contentType,
                                     const MiniCurlOptions* options);
MiniCurlResponse* mini_curl_c_put(MiniCurlHandle handle,
                                    const char* url,
                                    const char* data,
                                    const char* contentType,
                                    const MiniCurlOptions* options);
MiniCurlResponse* mini_curl_c_delete(MiniCurlHandle handle,
                                       const char* url,
                                       const MiniCurlOptions* options);
MiniCurlResponse* mini_curl_c_patch(MiniCurlHandle handle,
                                      const char* url,
                                      const char* data,
                                      const char* contentType,
                                      const MiniCurlOptions* options);
MiniCurlResponse* mini_curl_c_request(MiniCurlHandle handle,
                                        const char* method,
                                        const char* url,
                                        const char* data,
                                        const char* contentType,
                                        const MiniCurlOptions* options);

// Giải phóng response
void mini_curl_c_response_free(MiniCurlResponse* response);
```

#### Structures

```c
typedef struct {
    int statusCode;
    char* body;           // Phải gọi mini_curl_c_response_free để giải phóng
    size_t bodySize;
    char* error;          // Phải gọi mini_curl_c_response_free để giải phóng
} MiniCurlResponse;

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
} MiniCurlOptions;
```

## Ví dụ

### Ví dụ 1: GET Request

```c
MiniCurlHandle curl = mini_curl_c_create();
mini_curl_c_init(curl);

MiniCurlOptions options = {0};
options.verifySSL = 1;
options.timeout = 30;

MiniCurlResponse* response = mini_curl_c_get(curl, "https://api.example.com/data", &options);

if (response && response->statusCode == 200) {
    printf("Response: %s\n", response->body);
}

mini_curl_c_response_free(response);
mini_curl_c_cleanup(curl);
mini_curl_c_destroy(curl);
```

### Ví dụ 2: POST Request với JSON

```c
MiniCurlHandle curl = mini_curl_c_create();
mini_curl_c_init(curl);

MiniCurlOptions options = {0};
options.verifySSL = 1;

const char* jsonData = "{\"name\":\"test\",\"value\":123}";
MiniCurlResponse* response = mini_curl_c_post(curl, 
                                              "https://api.example.com/data",
                                              jsonData,
                                              "application/json",
                                              &options);

if (response && response->statusCode == 200) {
    printf("Success: %s\n", response->body);
}

mini_curl_c_response_free(response);
mini_curl_c_cleanup(curl);
mini_curl_c_destroy(curl);
```

### Ví dụ 6: Custom Headers

```cpp
WinHttpCurl http;
http.Init();

HttpOptions options;
options.headers["Authorization"] = "Bearer your-token";
options.headers["X-Custom-Header"] = "custom-value";
options.verifySSL = true;

HttpResponse response = http.Get("https://api.example.com/data", options);
```

### Ví dụ 7: Skip SSL Verification (Development only)

```c
MiniCurlOptions options = {0};
options.verifySSL = 0;  // Skip SSL verification (chỉ dùng khi development)
options.timeout = 30;
```

## Lưu ý

1. **Memory Management:**
   - C API: Phải gọi `mini_curl_c_response_free()` để giải phóng response
   - C++ API: Response được tự động giải phóng khi ra khỏi scope

2. **SSL Verification:**
   - Mặc định: `verifySSL = true` (verify SSL certificate)
   - Chỉ set `verifySSL = false` khi development/testing

3. **DLL Usage:**
   - Khi dùng DLL, phải copy DLL cùng với executable
   - DLL phải có trong cùng thư mục hoặc trong PATH

4. **Error Handling:**
   - Luôn kiểm tra `statusCode` trước khi sử dụng response
   - Kiểm tra `response->error` nếu có lỗi

## Troubleshooting

### Lỗi: "Failed to load DLL"
- Đảm bảo DLL có trong cùng thư mục với executable
- Hoặc thêm DLL vào PATH

### Lỗi: "SSL certificate verification failed"
- Set `verifySSL = false` (chỉ dùng khi development)
- Hoặc cài đặt certificate đúng cách

### Lỗi: "Timeout"
- Tăng `timeout` trong options
- Kiểm tra kết nối mạng

## Examples

Xem các ví dụ trong:
- `examples/example_curl_dll_direct.c` - Load DLL trực tiếp
- `examples/example_full_app.c` - Ví dụ đầy đủ kết hợp với mini_http_server

