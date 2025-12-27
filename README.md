# Call API - HTTP Client và Server Libraries

Dự án này bao gồm 2 thư viện chính:

- **mini_curl**: HTTP Client library sử dụng WinHTTP (Windows)
- **mini_http_server**: HTTP Server library với hỗ trợ OIDC callback, sử dụng Mongoose

## Cấu trúc dự án

```
call-api/
├── mini_curl/                    # HTTP Client Library
│   ├── include/
│   │   ├── winhttp_curl.h        # C++ API header
│   │   └── mini_curl_c.h         # C API header (cho DLL)
│   ├── src/
│   │   ├── winhttp_curl.cpp      # C++ implementation
│   │   └── mini_curl_c.cpp       # C API implementation
│   ├── examples/
│   │   ├── api_client.h          # C++ helper class
│   │   ├── api_client_c.h        # C wrapper header
│   │   └── api_client_c.cpp      # C wrapper implementation
│   └── README.md                 # Tài liệu chi tiết
│
├── mini_http_server/             # HTTP Server Library
│   ├── include/
│   │   ├── mini_http_server.h    # C API header
│   │   ├── oidc_config.h         # OIDC config (internal)
│   │   └── oidc_token_exchange.h # OIDC token exchange (internal)
│   ├── src/
│   │   ├── mini_http_server_c.cpp # C wrapper implementation
│   │   ├── oidc_config.cpp
│   │   └── oidc_token_exchange.cpp
│   ├── examples/
│   │   ├── example_simple.c      # Simple HTTP server example (C)
│   │   └── example_oidc.c        # OIDC callback server example (C)
│   └── external/                 # External dependencies (tự động tải khi build)
│       └── mongoose/             # Mongoose HTTP server (từ GitHub)
│
└── CMakeLists.txt                # Root CMakeLists - build cả 2 projects
```

## Yêu cầu

- **CMake** >= 3.14
- **MinGW-w64** với g++ compiler
- **Windows** (vì sử dụng WinHTTP)

## Build

### Build tất cả projects

```bash
# Build
build_cmake.bat

# Clean và build lại
build_cmake.bat clean
build_cmake.bat
```

### Build từng project riêng

#### Build mini_curl

```bash
cd mini_curl
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
```

#### Build mini_http_server

```bash
cd mini_http_server
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
```

**Lưu ý**: `mini_http_server` phụ thuộc vào `mini_curl`, nên cần build từ root `CMakeLists.txt` hoặc đảm bảo `mini_curl` đã được build trước.

## Output

Sau khi build, các file sẽ nằm trong `build/bin/`:

- `example_curl_dll_direct.exe` - mini_curl DLL example (load DLL trực tiếp)
- `example_full_app.exe` - Full app example (kết hợp cả 2 libraries)
- `example_simple.exe` - mini_http_server simple example (C)
- `example_oidc.exe` - mini_http_server OIDC example (C)

Libraries sẽ nằm trong `build/lib/`:
- `libmini_curl.a` - mini_curl static library (khi build static)
- `libmini_curl.dll` - mini_curl DLL (khi build DLL, nằm trong `build/bin/`)
- `libmini_http_server.a` - mini_http_server static library

## Sử dụng

### mini_curl - HTTP Client

Xem tài liệu chi tiết: [mini_curl/README.md](mini_curl/README.md)

#### C++ API (Static Library)

```cpp
#include "winhttp_curl.h"

WinHttpCurl http;
http.Init();

HttpOptions options;
options.verifySSL = true;
options.timeout = 30;

HttpResponse response = http.Get("https://api.example.com/data", options);
if (response.statusCode == 200) {
    printf("Response: %s\n", response.body.c_str());
}

http.Cleanup();
```

#### C API (Static Library)

```c
#include "mini_curl_c.h"

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

#### Load DLL Trực Tiếp (Không cần link)

```c
#include <windows.h>
#include "mini_curl_c.h"

// Load DLL và get function pointers
HMODULE dll = LoadLibraryA("libmini_curl.dll");
create_fn create = (create_fn)GetProcAddress(dll, "mini_curl_c_create");
// ... sử dụng functions
```

Xem `examples/example_curl_dll_direct.c` để biết ví dụ đầy đủ.

### mini_http_server - HTTP Server

#### Simple HTTP Server (C)

```c
#include "mini_http_server.h"

int main() {
    // Start simple HTTP server
    MiniHttpServer_StartSimple("http://localhost:8080", NULL);
    return 0;
}
```

#### OIDC Callback Server (C)

```c
#include "mini_http_server.h"

// Token callback
void on_token_received(
    const char* access_token,
    const char* refresh_token,
    const char* id_token,
    const char* email,
    const char* name,
    const char* username,
    void* user_data
) {
    printf("Access Token: %s\n", access_token);
    // ... xử lý token
}

// Error callback
void on_error(const char* error, const char* error_description, void* user_data) {
    printf("Error: %s\n", error);
}

int main() {
    // Load config
    MiniHttpServerConfig* config = MiniHttpServerConfig_New();
    MiniHttpServerConfig_LoadFromFile(config, "config.json");
    
    // Start server với callbacks
    MiniHttpServer_Start(config, on_token_received, on_error, NULL);
    
    MiniHttpServerConfig_Free(config);
    return 0;
}
```

Xem `mini_http_server/examples/` để biết các ví dụ đầy đủ.

## Configuration

### OIDC Configuration

Có thể cấu hình OIDC qua:

1. **Config file (JSON)**:
```json
{
  "token_endpoint": "https://auth.example.com/token",
  "redirect_uri": "http://localhost:8085/callback",
  "client_id": "your-client-id",
  "client_secret": "your-secret",
  "token_file": "token.json",
  "verify_ssl": true,
  "save_token": false,
  "listening_addr": "http://0.0.0.0:8085"
}
```

2. **Environment variables**:
```bash
set OIDC_TOKEN_ENDPOINT=https://auth.example.com/token
set OIDC_CLIENT_ID=your-client-id
set OIDC_REDIRECT_URI=http://localhost:8085/callback
```

3. **Command line parameters**:
```bash
example_oidc.exe http://0.0.0.0:8085 https://auth.example.com/token client-id client-secret http://localhost:8085/callback
```

## External Dependencies

### Mongoose

Mongoose HTTP server library được tự động tải từ GitHub khi build (sử dụng CMake FetchContent).

- Repository: https://github.com/cesanta/mongoose.git
- Version: 7.20
- Location: `build/_deps/mongoose-src/` (tự động tải)

## Build Options

### Static vs Shared Library

Mặc định, cả 2 libraries được build dưới dạng **static library** (không cần DLL).

Để build dưới dạng shared library (DLL):

```bash
cmake .. -DBUILD_SHARED_LIBS=ON
cmake --build .
```

## Development

### Project Structure

- `include/` - Public headers (API)
- `src/` - Source files (implementation)
- `examples/` - Example code
- `external/` - External dependencies (tự động tải khi build)

### Code Style

- C++ code: C++11 standard
- C code: C99 compatible
- Naming: snake_case cho C, camelCase cho C++
- Comments: Tiếng Việt cho documentation

## Troubleshooting

### Build Errors

1. **CMake không tìm thấy MinGW**:
   - Đảm bảo MinGW-w64 đã được cài đặt và thêm vào PATH
   - Kiểm tra: `g++ --version`

2. **Mongoose không tải được**:
   - Kiểm tra kết nối internet
   - Xóa `build/_deps/` và build lại

3. **Link errors với mini_curl**:
   - Đảm bảo build từ root `CMakeLists.txt` để có đầy đủ dependencies

### Runtime Errors

1. **SSL certificate errors**:
   - Set `verifySSL = false` trong options (chỉ dùng cho development)

2. **Port already in use**:
   - Đổi port trong `listening_addr` config

## License

[Thêm license information nếu có]

## Contributing

[Thêm contributing guidelines nếu có]

