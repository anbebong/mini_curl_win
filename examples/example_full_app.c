// Full Application Example - Kết hợp mini_http_server và mini_curl
//
// Ví dụ này demo cách:
// 1. Khởi động HTTP server để nhận OIDC callback và lấy token
// 2. Sử dụng token đó để gọi API bằng mini_curl
//
// Build: cmake --build build
// Run: .\build\bin\example_full_app.exe [config_file]
//   hoặc: .\build\bin\example_full_app.exe [listening_addr] [token_endpoint] [client_id] [client_secret] [redirect_uri] [api_base_url]

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <shellapi.h>
#include <time.h>
#include "mini_http_server.h"
#include "../mini_curl/include/mini_curl_c.h"

// Sử dụng static library - dùng trực tiếp functions
#ifdef MINI_CURL_STATIC
    // Dùng trực tiếp functions từ static library
    #define CURL_CREATE() mini_curl_c_create()
    #define CURL_DESTROY(h) mini_curl_c_destroy(h)
    #define CURL_INIT(h) mini_curl_c_init(h)
    #define CURL_CLEANUP(h) mini_curl_c_cleanup(h)
    #define CURL_GET(h, url, opts) mini_curl_c_get(h, url, opts)
    #define CURL_FREE(r) mini_curl_c_response_free(r)
#else
    // DLL loading - sử dụng function pointers
    typedef MiniCurlHandle (*create_fn)(void);
    typedef void (*destroy_fn)(MiniCurlHandle);
    typedef int (*init_fn)(MiniCurlHandle);
    typedef void (*cleanup_fn)(MiniCurlHandle);
    typedef MiniCurlResponse* (*get_fn)(MiniCurlHandle, const char*, const MiniCurlOptions*);
    typedef void (*free_fn)(MiniCurlResponse*);

    // Global DLL handle và function pointers
    static HMODULE g_dll_handle = NULL;
    static create_fn g_curl_create = NULL;
    static destroy_fn g_curl_destroy = NULL;
    static init_fn g_curl_init = NULL;
    static cleanup_fn g_curl_cleanup = NULL;
    static get_fn g_curl_get = NULL;
    static free_fn g_curl_free = NULL;
    
    #define CURL_CREATE() g_curl_create()
    #define CURL_DESTROY(h) g_curl_destroy(h)
    #define CURL_INIT(h) g_curl_init(h)
    #define CURL_CLEANUP(h) g_curl_cleanup(h)
    #define CURL_GET(h, url, opts) g_curl_get(h, url, opts)
    #define CURL_FREE(r) g_curl_free(r)
#endif

// Global variables
// Token có thể rất dài (JWT tokens thường 1000-2000+ ký tự), tăng lên 8192 để an toàn
static char g_access_token[8192] = {0};
static char g_api_base_url[512] = {0};
static int g_token_received = 0;
static int g_need_reauth = 0;  // Flag để yêu cầu re-authentication
static CRITICAL_SECTION g_token_cs;
static char g_state[64] = {0};
static char g_nonce[64] = {0};
static MiniHttpServerConfig* g_oidc_config = NULL;  // Lưu config để có thể re-authenticate

// Forward declarations
#ifndef MINI_CURL_STATIC
int LoadMiniCurlDLL(const char* dll_path);
void UnloadMiniCurlDLL(void);
#endif
int is_token_valid(void);
void clear_token(void);
void trigger_reauth(void);
void call_api_with_token(void);
void generate_uuid(char* output, size_t size);
int build_oidc_auth_url(const MiniHttpServerConfig* config, char* auth_url, size_t auth_url_size);
int open_browser(const char* url);

// Kiểm tra token hợp lệ (không rỗng, có format cơ bản)
int is_token_valid(void) {
    EnterCriticalSection(&g_token_cs);
    int valid = (g_token_received && strlen(g_access_token) > 0);
    LeaveCriticalSection(&g_token_cs);
    return valid;
}

// Clear token (khi token hết hạn hoặc không hợp lệ)
void clear_token(void) {
    EnterCriticalSection(&g_token_cs);
    g_access_token[0] = '\0';
    g_token_received = 0;
    LeaveCriticalSection(&g_token_cs);
    printf("Token cleared. Need to re-authenticate.\n");
}

// Trigger re-authentication
void trigger_reauth(void) {
    clear_token();
    g_need_reauth = 1;
    
    if (g_oidc_config) {
        // Build OIDC authorization URL
        char auth_url[1024] = {0};
        if (build_oidc_auth_url(g_oidc_config, auth_url, sizeof(auth_url))) {
            printf("\n=== Re-authentication Required ===\n");
            printf("Opening browser for authentication...\n");
            printf("OIDC Authorization URL:\n%s\n\n", auth_url);
            
            if (open_browser(auth_url)) {
                printf("Browser opened successfully.\n");
            } else {
                printf("Warning: Failed to open browser automatically.\n");
                printf("Please open the URL above manually in your browser.\n");
            }
        } else {
            printf("Error: Failed to build OIDC authorization URL.\n");
        }
    } else {
        printf("Error: OIDC config not available for re-authentication.\n");
    }
}

// Token callback - được gọi khi nhận được token từ OIDC
void on_token_received(
    const char* access_token,
    const char* refresh_token,
    const char* id_token,
    const char* email,
    const char* name,
    const char* username,
    void* user_data
) {
    EnterCriticalSection(&g_token_cs);
    
    printf("\n=== Token Received ===\n");
    printf("Access Token: %.50s...\n", access_token ? access_token : "(null)");
    if (refresh_token) {
        printf("Refresh Token: %.50s...\n", refresh_token);
    }
    if (email) {
        printf("Email: %s\n", email);
    }
    if (name) {
        printf("Name: %s\n", name);
    }
    if (username) {
        printf("Username: %s\n", username);
    }
    printf("=====================\n\n");
    
    // Lưu access token để dùng cho API calls
    if (access_token) {
        strncpy(g_access_token, access_token, sizeof(g_access_token) - 1);
        g_access_token[sizeof(g_access_token) - 1] = '\0';
        g_token_received = 1;
        g_need_reauth = 0;  // Reset re-auth flag
    }
    
    LeaveCriticalSection(&g_token_cs);
    
    // Tự động gọi API sau khi nhận được token
    printf("Calling API with received token...\n");
    call_api_with_token();
}

// Error callback
void on_error(const char* error, const char* error_description, void* user_data) {
    printf("\n=== OIDC Error ===\n");
    printf("Error: %s\n", error);
    if (error_description) {
        printf("Description: %s\n", error_description);
    }
    printf("==================\n\n");
}

#ifndef MINI_CURL_STATIC
// Load mini_curl DLL (chỉ cần khi dùng DLL)
int LoadMiniCurlDLL(const char* dll_path) {
    if (g_dll_handle) {
        return 1; // Đã load rồi
    }
    
    // Load DLL
    g_dll_handle = LoadLibraryA(dll_path ? dll_path : "libmini_curl.dll");
    if (!g_dll_handle) {
        DWORD error = GetLastError();
        printf("Error: Failed to load libmini_curl.dll (Error: %lu)\n", error);
        printf("Make sure libmini_curl.dll is in the same directory or in PATH\n");
        return 0;
    }
    
    // Get function addresses
    g_curl_create = (create_fn)GetProcAddress(g_dll_handle, "mini_curl_c_create");
    g_curl_destroy = (destroy_fn)GetProcAddress(g_dll_handle, "mini_curl_c_destroy");
    g_curl_init = (init_fn)GetProcAddress(g_dll_handle, "mini_curl_c_init");
    g_curl_cleanup = (cleanup_fn)GetProcAddress(g_dll_handle, "mini_curl_c_cleanup");
    g_curl_get = (get_fn)GetProcAddress(g_dll_handle, "mini_curl_c_get");
    g_curl_free = (free_fn)GetProcAddress(g_dll_handle, "mini_curl_c_response_free");
    
    if (!g_curl_create || !g_curl_destroy || !g_curl_init || !g_curl_cleanup || !g_curl_get || !g_curl_free) {
        printf("Error: Failed to get function addresses from DLL\n");
        FreeLibrary(g_dll_handle);
        g_dll_handle = NULL;
        return 0;
    }
    
    return 1;
}

// Unload mini_curl DLL
void UnloadMiniCurlDLL(void) {
    if (g_dll_handle) {
        FreeLibrary(g_dll_handle);
        g_dll_handle = NULL;
        g_curl_create = NULL;
        g_curl_destroy = NULL;
        g_curl_init = NULL;
        g_curl_cleanup = NULL;
        g_curl_get = NULL;
        g_curl_free = NULL;
    }
}
#endif

// Gọi API sử dụng token đã nhận được
void call_api_with_token(void) {
    // Kiểm tra token hợp lệ trước khi gọi API
    if (!is_token_valid()) {
        printf("Error: No valid access token available.\n");
        trigger_reauth();
        return;
    }
    
    if (strlen(g_api_base_url) == 0) {
        printf("Warning: API base URL not set. Skipping API call.\n");
        return;
    }
    
#ifndef MINI_CURL_STATIC
    // Đảm bảo DLL đã được load (chỉ cần khi dùng DLL)
    if (!g_dll_handle && !LoadMiniCurlDLL(NULL)) {
        printf("Error: Failed to load mini_curl DLL\n");
        return;
    }
#endif
    
    // Tạo HTTP client
    MiniCurlHandle curl = CURL_CREATE();
    if (!curl) {
        printf("Failed to create HTTP client!\n");
        return;
    }
    
    // Khởi tạo client
    if (!CURL_INIT(curl)) {
        printf("Failed to initialize HTTP client!\n");
        CURL_DESTROY(curl);
        return;
    }
    
    // Example 5: Lấy danh sách applications (GET /app/agent)
    printf("[5] Lay danh sach applications (GET /app/agent)...\n");
    printf("Note: Can X-Auth-Token header. Neu chua co token, se bao loi 401.\n");
    
    // Build URL
    char url[1024];
    snprintf(url, sizeof(url), "%s/app/agent", g_api_base_url);
    
    // Cấu hình options với X-Auth-Token header
    // Token có thể dài, cần đủ chỗ cho "X-Auth-Token: " + token (2048) + null terminator
    char customHeaders[4096];
    snprintf(customHeaders, sizeof(customHeaders), "X-Auth-Token: %s", g_access_token);
    
    MiniCurlOptions options = {0};
    options.verifySSL = 1;
    options.timeout = 30;
    options.customHeaders = customHeaders;
    
    // Gọi GET request
    printf("Calling: %s\n", url);
    printf("Using token: %.20s...\n", g_access_token);
    
    MiniCurlResponse* response = CURL_GET(curl, url, &options);
    
    // Kiểm tra kết quả
    if (response) {
        if (response->statusCode == 200) {
            printf("Success: Applications retrieved successfully!\n");
            printf("Response length: %zu bytes\n", response->bodySize);
            
            // Lưu response vào file để xem
            FILE* appsFile = fopen("applications.json", "w");
            if (appsFile) {
                if (response->body) {
                    fprintf(appsFile, "%s", response->body);
                }
                fclose(appsFile);
                printf("Success: Response saved to applications.json\n");
            }
            
            // In một phần response để xem (nếu quá dài)
            if (response->body && response->bodySize > 500) {
                printf("Response preview (first 500 chars):\n%.500s...\n", response->body);
            } else if (response->body) {
                printf("Response:\n%s\n", response->body);
            }
        } else if (response->statusCode == 401) {
            // Token hết hạn hoặc không hợp lệ - cần re-authenticate
            printf("Error: HTTP 401 Unauthorized - Token expired or invalid.\n");
            if (response->body) {
                printf("  Response Body: %s\n", response->body);
            }
            printf("  Triggering re-authentication...\n");
            
            // Clear token và trigger re-authentication
            clear_token();
            trigger_reauth();
        } else {
            printf("Error: HTTP Status Code: %d\n", response->statusCode);
            if (response->error) {
                printf("  Error Message: %s\n", response->error);
            }
            if (response->body) {
                printf("  Response Body: %s\n", response->body);
            }
        }
        
        // Giải phóng response
        CURL_FREE(response);
    } else {
        printf("Error: Failed to get response\n");
    }
    
    printf("\n");
    printf("=== Done ===\n");
    
    // Cleanup
    CURL_CLEANUP(curl);
    CURL_DESTROY(curl);
}

// Cleanup khi exit
void cleanup_on_exit(void) {
#ifndef MINI_CURL_STATIC
    UnloadMiniCurlDLL();
#endif
    DeleteCriticalSection(&g_token_cs);
}

// Generate UUID v4 (simple version)
void generate_uuid(char* output, size_t size) {
    if (!output || size < 37) return; // UUID cần 36 chars + null terminator
    
    srand((unsigned int)time(NULL));
    
    // Format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
    snprintf(output, size,
        "%08x-%04x-4%03x-%04x-%012x",
        rand() & 0xffffffff,
        rand() & 0xffff,
        rand() & 0x0fff,
        (rand() & 0x3fff) | 0x8000, // y = 8, 9, a, hoặc b
        ((unsigned long long)rand() << 32) | rand()
    );
}

// Build OIDC authorization URL
// Tương tự Go code: config.AuthCodeURL(state, oidc.Nonce(nonce))
int build_oidc_auth_url(const MiniHttpServerConfig* config, char* auth_url, size_t auth_url_size) {
    if (!config || !config->token_endpoint || !config->client_id || !config->redirect_uri) {
        return 0;
    }
    
    // Extract base URL từ token_endpoint
    // token_endpoint thường có dạng: https://auth.example.com/id/realms/realm/protocol/openid-connect/token
    // Cần extract: https://auth.example.com/id/realms/realm
    char auth_server_url[512] = {0};
    strncpy(auth_server_url, config->token_endpoint, sizeof(auth_server_url) - 1);
    
    // Tìm và remove phần /protocol/openid-connect/token
    char* protocol_pos = strstr(auth_server_url, "/protocol/openid-connect/token");
    if (protocol_pos) {
        *protocol_pos = '\0';
    }
    
    // Đảm bảo có /id nếu chưa có
    if (strstr(auth_server_url, "/id") == NULL) {
        // Nếu có /realms thì insert /id trước /realms
        char* realms_pos = strstr(auth_server_url, "/realms");
        if (realms_pos) {
            size_t pos = realms_pos - auth_server_url;
            char temp[512];
            strncpy(temp, auth_server_url, pos);
            temp[pos] = '\0';
            strcat(temp, "/id");
            strcat(temp, realms_pos);
            strncpy(auth_server_url, temp, sizeof(auth_server_url) - 1);
        } else {
            // Thêm /id vào cuối
            strncat(auth_server_url, "/id", sizeof(auth_server_url) - 1);
        }
    }
    
    // Generate state và nonce
    static char g_state[64] = {0};
    static char g_nonce[64] = {0};
    generate_uuid(g_state, sizeof(g_state));
    generate_uuid(g_nonce, sizeof(g_nonce));
    
    // Build authorization URL
    // Format: {auth_server}/realms/{realm}/protocol/openid-connect/auth?client_id=...&redirect_uri=...&response_type=code&scope=...&state=...&nonce=...
    snprintf(auth_url, auth_url_size,
        "%s/protocol/openid-connect/auth?client_id=%s&redirect_uri=%s&response_type=code&scope=openid%%20offline_access%%20email%%20profile%%20api&state=%s&nonce=%s",
        auth_server_url,
        config->client_id,
        config->redirect_uri ? config->redirect_uri : "http://localhost:8085/callback",
        g_state,
        g_nonce
    );
    
    return 1;
}

// Mở browser với URL (Windows)
int open_browser(const char* url) {
    if (!url) return 0;
    
    // Sử dụng ShellExecute để mở browser
    HINSTANCE result = ShellExecuteA(
        NULL,           // hwnd
        "open",         // lpOperation
        url,            // lpFile
        NULL,           // lpParameters
        NULL,           // lpDirectory
        SW_SHOWNORMAL   // nShowCmd
    );
    
    // ShellExecute trả về HINSTANCE > 32 nếu thành công
    return ((INT_PTR)result > 32) ? 1 : 0;
}

// Thread function để chạy HTTP server
DWORD WINAPI server_thread(LPVOID lpParam) {
    MiniHttpServerConfig* config = (MiniHttpServerConfig*)lpParam;
    
    printf("Starting OIDC callback server in background thread...\n");
    printf("Waiting for OIDC callback...\n");
    printf("Callback URL: %s/callback\n", config->listening_addr);
    
    // Build OIDC authorization URL
    char auth_url[1024] = {0};
    if (build_oidc_auth_url(config, auth_url, sizeof(auth_url))) {
        printf("\nOIDC Authorization URL:\n%s\n\n", auth_url);
        
        // Mở browser tự động
        printf("Opening browser for authentication...\n");
        if (open_browser(auth_url)) {
            printf("Browser opened successfully.\n");
        } else {
            printf("Warning: Failed to open browser automatically.\n");
            printf("Please open the URL above manually in your browser.\n");
        }
    } else {
        printf("Warning: Failed to build OIDC authorization URL.\n");
        printf("Please authenticate manually.\n");
    }
    
    printf("\n");
    
    // Start server (blocking call)
    MiniHttpServer_Start(config, on_token_received, on_error, NULL);
    
    return 0;
}

int main(int argc, char *argv[]) {
    // Initialize critical section
    InitializeCriticalSection(&g_token_cs);
    
#ifdef MINI_CURL_STATIC
    printf("Using mini_curl static library.\n\n");
#else
    // Load mini_curl DLL (chỉ cần khi dùng DLL)
    if (!LoadMiniCurlDLL(NULL)) {
        printf("Warning: Failed to load mini_curl DLL. API calls may fail.\n");
        printf("Make sure libmini_curl.dll is in the same directory or in PATH.\n\n");
    }
#endif
    
    // Parse API base URL từ command line (optional)
    if (argc > 6) {
        strncpy(g_api_base_url, argv[6], sizeof(g_api_base_url) - 1);
        g_api_base_url[sizeof(g_api_base_url) - 1] = '\0';
    } else {
        // Default API base URL (có thể thay đổi)
        strncpy(g_api_base_url, "https://uat.lab.linksafe.vn/pam-api", sizeof(g_api_base_url) - 1);
        printf("Warning: API base URL not provided, using default: %s\n", g_api_base_url);
        printf("Usage: %s [listening_addr] [token_endpoint] [client_id] [client_secret] [redirect_uri] [api_base_url]\n", argv[0]);
        printf("   or: %s [config_file] [api_base_url]\n\n", argv[0]);
    }
    
    // Load OIDC configuration
    MiniHttpServerConfig* config = MiniHttpServerConfig_New();
    if (!config) {
        fprintf(stderr, "Failed to create config\n");
        cleanup_on_exit();
        return 1;
    }
    
    // Lưu config để có thể re-authenticate sau này
    g_oidc_config = config;
    
    bool configLoaded = false;
    
    // Try to load from config file
    if (argc >= 2) {
        const char* arg1 = argv[1];
        // Nếu không phải là URL, coi như là config file
        if (strstr(arg1, "http://") != arg1 && strstr(arg1, "https://") != arg1) {
            if (MiniHttpServerConfig_LoadFromFile(config, arg1)) {
                configLoaded = true;
                printf("Configuration loaded from file: %s\n", arg1);
                
                // API base URL có thể là argument thứ 2 nếu load từ file
                if (argc > 2 && strlen(g_api_base_url) == 0) {
                    strncpy(g_api_base_url, argv[2], sizeof(g_api_base_url) - 1);
                    g_api_base_url[sizeof(g_api_base_url) - 1] = '\0';
                }
            } else {
                fprintf(stderr, "Warning: Failed to load config from file: %s\n", arg1);
            }
        }
    }
    
    // Load from parameters if not loaded from file
    if (!configLoaded) {
        const char* tokenEndpoint = (argc > 2) ? argv[2] : NULL;
        const char* clientId = (argc > 3) ? argv[3] : NULL;
        const char* clientSecret = (argc > 4) ? argv[4] : NULL;
        const char* redirectUri = (argc > 5) ? argv[5] : NULL;
        const char* tokenFile = NULL;
        int verifySSL = -1;
        int saveToken = 0; // Không lưu token vào file trong example này
        
        const char* listeningAddr = (argc > 1) ? argv[1] : NULL;
        
        if (!MiniHttpServerConfig_LoadFromParams(
            config,
            tokenEndpoint,
            redirectUri,
            clientId,
            clientSecret,
            tokenFile,
            verifySSL,
            saveToken,
            listeningAddr
        )) {
            fprintf(stderr, "Error: Missing required configuration (token_endpoint and client_id)\n");
            fprintf(stderr, "\nUsage:\n");
            fprintf(stderr, "  %s [config_file] [api_base_url]\n", argv[0]);
            fprintf(stderr, "  %s [listening_addr] [token_endpoint] [client_id] [client_secret] [redirect_uri] [api_base_url]\n", argv[0]);
            fprintf(stderr, "\nExample:\n");
            fprintf(stderr, "  %s config.json https://api.example.com\n", argv[0]);
            fprintf(stderr, "  %s http://0.0.0.0:8085 https://auth.example.com/token client-id secret http://localhost:8085/callback https://api.example.com\n", argv[0]);
            MiniHttpServerConfig_Free(config);
            cleanup_on_exit();
            return 1;
        }
    }
    
    printf("\n=== Full Application Example ===\n");
    printf("OIDC Configuration:\n");
    printf("  Listening Address: %s\n", config->listening_addr);
    printf("  Redirect URI: %s\n", config->redirect_uri ? config->redirect_uri : "(not set)");
    printf("  Token Endpoint: %s\n", config->token_endpoint ? config->token_endpoint : "(not set)");
    printf("  Client ID: %s\n", config->client_id ? config->client_id : "(not set)");
    printf("\nAPI Configuration:\n");
    printf("  API Base URL: %s\n", g_api_base_url);
    printf("\n");
    
    // Start server trong background thread
    HANDLE hThread = CreateThread(NULL, 0, server_thread, config, 0, NULL);
    if (hThread == NULL) {
        fprintf(stderr, "Error: Failed to create server thread\n");
        MiniHttpServerConfig_Free(config);
        cleanup_on_exit();
        return 1;
    }
    
    printf("Server started. Waiting for authentication...\n");
    printf("Press Ctrl+C to stop.\n\n");
    
    // Main loop - chờ token và xử lý re-authentication
    while (1) {
        // Kiểm tra nếu cần re-authenticate
        if (g_need_reauth && !g_token_received) {
            // Đã trigger re-auth, chờ token mới
            Sleep(1000);
            continue;
        }
        
        // Chờ token nếu chưa có
        if (!g_token_received) {
            Sleep(1000); // Check every second
            continue;
        }
        
        // Token đã có, cho phép user gọi API
        if (g_token_received && !g_need_reauth) {
            printf("\nToken received! Application ready.\n");
            printf("You can now call APIs using the access token.\n");
            printf("Press Enter to call API, or Ctrl+C to exit...\n");
            
            // Wait for user input
            getchar();
            
            // Kiểm tra token trước khi gọi API
            if (is_token_valid()) {
                call_api_with_token();
            } else {
                printf("Token invalid. Triggering re-authentication...\n");
                trigger_reauth();
            }
        }
    }
    
    // Cleanup (sẽ không đến đây vì loop vô hạn, nhưng để đảm bảo)
    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
    MiniHttpServerConfig_Free(config);
    cleanup_on_exit();
    
    return 0;
}

