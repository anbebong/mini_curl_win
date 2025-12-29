// Mini HTTP Server Library - C API
//
// Thư viện HTTP server đơn giản sử dụng Mongoose với hỗ trợ OIDC callback
//
// Build library: cmake --build build
// Use in C code: #include "mini_http_server.h"

#ifndef MINI_HTTP_SERVER_H
#define MINI_HTTP_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

// OIDC Configuration structure (C-compatible)
typedef struct {
    char* token_endpoint;      // Token endpoint URL
    char* redirect_uri;         // Redirect URI đã đăng ký
    char* client_id;            // OIDC Client ID
    char* client_secret;        // OIDC Client Secret (có thể NULL cho public client)
    char* token_file;           // File để lưu tokens (NULL = không lưu)
    bool verify_ssl;            // true = verify SSL, false = skip
    bool save_token;            // true = lưu token vào file, false = chỉ in ra console
    char* listening_addr;       // Listening address (ví dụ: "http://localhost:8085")
} MiniHttpServerConfig;

// Callback function type cho OIDC token exchange success
// Parameters:
//   access_token - Access token nhận được
//   refresh_token - Refresh token (có thể NULL)
//   id_token - ID token (có thể NULL)
//   email - Email từ ID token (có thể NULL)
//   name - Name từ ID token (có thể NULL)
//   username - Username từ ID token (có thể NULL)
//   user_data - User data pointer được truyền vào StartServer
typedef void (*MiniHttpServerTokenCallback)(
    const char* access_token,
    const char* refresh_token,
    const char* id_token,
    const char* email,
    const char* name,
    const char* username,
    void* user_data
);

// Callback function type cho OIDC error
// Parameters:
//   error - Error code
//   error_description - Error description (có thể NULL)
//   user_data - User data pointer được truyền vào StartServer
typedef void (*MiniHttpServerErrorCallback)(
    const char* error,
    const char* error_description,
    void* user_data
);

// Initialize configuration với default values
// Returns: pointer to config structure (phải gọi FreeConfig để giải phóng)
MiniHttpServerConfig* MiniHttpServerConfig_New(void);

// Load configuration từ file JSON
// Parameters:
//   config - Config structure (đã được New)
//   filename - Đường dẫn đến file JSON
// Returns: true nếu thành công, false nếu có lỗi
bool MiniHttpServerConfig_LoadFromFile(MiniHttpServerConfig* config, const char* filename);

// Load configuration từ parameters
// Parameters:
//   config - Config structure (đã được New)
//   token_endpoint - Token endpoint (có thể NULL để dùng env var)
//   redirect_uri - Redirect URI (có thể NULL để dùng env var)
//   client_id - Client ID (có thể NULL để dùng env var)
//   client_secret - Client Secret (có thể NULL)
//   token_file - Token file path (có thể NULL)
//   verify_ssl - Verify SSL flag (-1 = dùng env var, 0 = false, 1 = true)
//   save_token - Save token flag (-1 = dùng env var, 0 = false, 1 = true)
//   listening_addr - Listening address (có thể NULL để dùng default)
// Returns: true nếu thành công, false nếu có lỗi
bool MiniHttpServerConfig_LoadFromParams(
    MiniHttpServerConfig* config,
    const char* token_endpoint,
    const char* redirect_uri,
    const char* client_id,
    const char* client_secret,
    const char* token_file,
    int verify_ssl,
    int save_token,
    const char* listening_addr
);

// Free configuration structure
void MiniHttpServerConfig_Free(MiniHttpServerConfig* config);

// Start HTTP server với OIDC callback support
// Parameters:
//   config - Configuration structure
//   token_callback - Callback được gọi khi token exchange thành công (có thể NULL)
//   error_callback - Callback được gọi khi có lỗi (có thể NULL)
//   user_data - User data pointer được truyền vào callbacks
// Returns: 0 nếu thành công, non-zero nếu có lỗi
//
// Server sẽ chạy cho đến khi nhận SIGINT hoặc SIGTERM
int MiniHttpServer_Start(
    const MiniHttpServerConfig* config,
    MiniHttpServerTokenCallback token_callback,
    MiniHttpServerErrorCallback error_callback,
    void* user_data
);

// Start simple HTTP server (không có OIDC)
// Parameters:
//   listening_addr - Listening address (ví dụ: "http://localhost:8080")
//   user_data - User data pointer (có thể NULL)
// Returns: 0 nếu thành công, non-zero nếu có lỗi
//
// Server sẽ chạy cho đến khi nhận SIGINT hoặc SIGTERM
int MiniHttpServer_StartSimple(const char* listening_addr, void* user_data);

#ifdef __cplusplus
}
#endif

#endif // MINI_HTTP_SERVER_H

