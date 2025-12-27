// OIDC Configuration Helper
// Đọc cấu hình OIDC từ file JSON hoặc parameters

#ifndef OIDC_CONFIG_H
#define OIDC_CONFIG_H

#include <string>

// Cấu trúc chứa OIDC configuration
struct OidcConfig {
    std::string token_endpoint;    // Token endpoint URL
    std::string redirect_uri;       // Redirect URI đã đăng ký
    std::string client_id;          // OIDC Client ID
    std::string client_secret;      // OIDC Client Secret (có thể empty cho public client)
    std::string token_file;        // File để lưu tokens (empty = không lưu file)
    bool verify_ssl;               // true = verify SSL, false = skip (chỉ dùng cho development)
    bool save_token;               // true = lưu token vào file, false = chỉ in ra console (default: false)
    
    // Server configuration
    std::string listening_addr;    // Listening address (ví dụ: "http://localhost:8085")
    
    OidcConfig() : verify_ssl(true), save_token(false), listening_addr("http://localhost:8085") {}
};

// Đọc cấu hình từ file JSON
//
// File JSON format:
// {
//   "token_endpoint": "https://auth.example.com/realms/realm/protocol/openid-connect/token",
//   "redirect_uri": "http://localhost:8085/callback",
//   "client_id": "your-client-id",
//   "client_secret": "your-secret",
//   "token_file": "token.json",
//   "verify_ssl": true,
//   "listening_addr": "http://localhost:8085"
// }
//
// Parameters:
//   filename - Đường dẫn đến file JSON config
//   config - OidcConfig struct để fill vào
//
// Returns:
//   true nếu đọc thành công, false nếu có lỗi
bool LoadOidcConfigFromFile(const std::string& filename, OidcConfig& config);

// Đọc cấu hình từ parameters (có thể kết hợp với environment variables)
//
// Priority:
//   1. Parameters (nếu không empty)
//   2. Environment variables
//   3. Default values
//
// Parameters:
//   tokenEndpoint - Token endpoint (có thể empty để dùng env var hoặc default)
//   redirectUri - Redirect URI (có thể empty để dùng env var hoặc default)
//   clientId - Client ID (có thể empty để dùng env var hoặc default)
//   clientSecret - Client Secret (có thể empty để dùng env var hoặc default)
//   tokenFile - Token file path (có thể empty để không lưu file)
//   verifySSL - Verify SSL flag (có thể -1 để dùng env var hoặc default)
//   saveToken - Có lưu token vào file không (có thể -1 để dùng env var hoặc default)
//   listeningAddr - Listening address (có thể empty để dùng env var hoặc default)
//   config - OidcConfig struct để fill vào
//
// Returns:
//   true nếu có ít nhất token_endpoint và client_id, false nếu thiếu
bool LoadOidcConfigFromParams(
    const std::string& tokenEndpoint,
    const std::string& redirectUri,
    const std::string& clientId,
    const std::string& clientSecret,
    const std::string& tokenFile,
    int verifySSL,  // -1 = use env/default, 0 = false, 1 = true
    int saveToken,  // -1 = use env/default, 0 = false, 1 = true
    const std::string& listeningAddr,
    OidcConfig& config
);

// Default values
#define DEFAULT_LISTENING_ADDR "http://localhost:8085"
#define DEFAULT_REDIRECT_URI "http://localhost:8085/callback"
#define DEFAULT_TOKEN_FILE "token.json"
#define DEFAULT_VERIFY_SSL true
#define DEFAULT_TOKEN_ENDPOINT "https://uat.lab.linksafe.vn/id/realms/main/protocol/openid-connect/token"
#define DEFAULT_CLIENT_ID "lzt-portal"
#define DEFAULT_CLIENT_SECRET ""

#endif // OIDC_CONFIG_H

