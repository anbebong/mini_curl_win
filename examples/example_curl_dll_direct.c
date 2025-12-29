// Ví dụ load mini_curl DLL trực tiếp (không cần wrapper)
//
// Ví dụ này demo cách load và sử dụng mini_curl DLL trực tiếp từ C code
// Sử dụng LoadLibrary/GetProcAddress để load DLL động
//
// Build DLL: build_cmake.bat dll
// Run: .\build\bin\example_curl_dll_direct.exe (cần có mini_curl.dll cùng thư mục)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "../mini_curl/include/mini_curl_c.h"

// Function pointer types
typedef MiniCurlHandle (*create_fn)(void);
typedef void (*destroy_fn)(MiniCurlHandle);
typedef int (*init_fn)(MiniCurlHandle);
typedef void (*cleanup_fn)(MiniCurlHandle);
typedef MiniCurlResponse* (*get_fn)(MiniCurlHandle, const char*, const MiniCurlOptions*);
typedef MiniCurlResponse* (*post_fn)(MiniCurlHandle, const char*, const char*, const char*, const MiniCurlOptions*);
typedef MiniCurlResponse* (*put_fn)(MiniCurlHandle, const char*, const char*, const char*, const MiniCurlOptions*);
typedef MiniCurlResponse* (*delete_fn)(MiniCurlHandle, const char*, const MiniCurlOptions*);
typedef MiniCurlResponse* (*patch_fn)(MiniCurlHandle, const char*, const char*, const char*, const MiniCurlOptions*);
typedef MiniCurlResponse* (*request_fn)(MiniCurlHandle, const char*, const char*, const char*, const char*, const MiniCurlOptions*);
typedef void (*free_fn)(MiniCurlResponse*);

// Global DLL handle và function pointers
static HMODULE g_dll_handle = NULL;
static create_fn g_create = NULL;
static destroy_fn g_destroy = NULL;
static init_fn g_init = NULL;
static cleanup_fn g_cleanup = NULL;
static get_fn g_get = NULL;
static post_fn g_post = NULL;
static put_fn g_put = NULL;
static delete_fn g_delete = NULL;
static patch_fn g_patch = NULL;
static request_fn g_request = NULL;
static free_fn g_free = NULL;

// Load DLL và get function pointers
int LoadMiniCurlDLL(const char* dll_path) {
    // Load DLL
    g_dll_handle = LoadLibraryA(dll_path ? dll_path : "libmini_curl.dll");
    if (!g_dll_handle) {
        DWORD error = GetLastError();
        printf("Error: Failed to load mini_curl.dll (Error: %lu)\n", error);
        printf("Make sure mini_curl.dll is in the same directory or in PATH\n");
        return 0;
    }
    
    // Get function addresses
    g_create = (create_fn)GetProcAddress(g_dll_handle, "mini_curl_c_create");
    g_destroy = (destroy_fn)GetProcAddress(g_dll_handle, "mini_curl_c_destroy");
    g_init = (init_fn)GetProcAddress(g_dll_handle, "mini_curl_c_init");
    g_cleanup = (cleanup_fn)GetProcAddress(g_dll_handle, "mini_curl_c_cleanup");
    g_get = (get_fn)GetProcAddress(g_dll_handle, "mini_curl_c_get");
    g_post = (post_fn)GetProcAddress(g_dll_handle, "mini_curl_c_post");
    g_put = (put_fn)GetProcAddress(g_dll_handle, "mini_curl_c_put");
    g_delete = (delete_fn)GetProcAddress(g_dll_handle, "mini_curl_c_delete");
    g_patch = (patch_fn)GetProcAddress(g_dll_handle, "mini_curl_c_patch");
    g_request = (request_fn)GetProcAddress(g_dll_handle, "mini_curl_c_request");
    g_free = (free_fn)GetProcAddress(g_dll_handle, "mini_curl_c_response_free");
    
    if (!g_create || !g_destroy || !g_init || !g_cleanup || !g_get || !g_post || 
        !g_put || !g_delete || !g_patch || !g_request || !g_free) {
        printf("Error: Failed to get function addresses from DLL\n");
        FreeLibrary(g_dll_handle);
        g_dll_handle = NULL;
        return 0;
    }
    
    printf("DLL loaded successfully!\n");
    return 1;
}

// Unload DLL
void UnloadMiniCurlDLL(void) {
    if (g_dll_handle) {
        FreeLibrary(g_dll_handle);
        g_dll_handle = NULL;
    }
}

int main() {
    printf("=== Mini Curl - Direct DLL Loading Example ===\n\n");
    printf("Note: This example loads mini_curl.dll directly using LoadLibrary\n");
    printf("Make sure mini_curl.dll is in the same directory as this .exe\n\n");
    
    // Load DLL
    if (!LoadMiniCurlDLL(NULL)) {
        return 1;
    }
    
    // Tạo HTTP client
    MiniCurlHandle curl = g_create();
    if (!curl) {
        printf("Error: Failed to create HTTP client\n");
        UnloadMiniCurlDLL();
        return 1;
    }
    
    // Khởi tạo client
    if (!g_init(curl)) {
        printf("Error: Failed to initialize HTTP client\n");
        g_destroy(curl);
        UnloadMiniCurlDLL();
        return 1;
    }
    
    printf("HTTP Client initialized (loaded from DLL).\n\n");
    
    // Cấu hình options
    MiniCurlOptions options = {0};
    options.verifySSL = 1;
    options.timeout = 30;
    
    // Gọi GET request
    printf("Calling GET https://httpbin.org/get...\n");
    MiniCurlResponse* response = g_get(curl, "https://httpbin.org/get", &options);
    
    // Kiểm tra kết quả
    if (response) {
        if (response->statusCode == 200) {
            printf("Success! Status Code: %d\n", response->statusCode);
            printf("\nResponse Body (first 500 chars):\n");
            
            if (response->bodySize > 500) {
                printf("%.500s...\n", response->body);
                printf("\n(Total length: %zu bytes)\n", response->bodySize);
            } else {
                printf("%s\n", response->body ? response->body : "(empty)");
            }
        } else {
            printf("Error! Status Code: %d\n", response->statusCode);
            if (response->error) {
                printf("Error Message: %s\n", response->error);
            }
        }
        
        // Giải phóng response
        g_free(response);
    } else {
        printf("Error: Failed to get response\n");
    }
    
    // Cleanup
    g_cleanup(curl);
    g_destroy(curl);
    UnloadMiniCurlDLL();
    
    printf("\n=== Done ===\n");
    return 0;
}
