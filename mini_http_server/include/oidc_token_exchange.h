// OIDC Token Exchange Helper
// Exchange authorization code thành access token, refresh token, và ID token
//
// This module provides functionality to:
// - Exchange OIDC authorization code for tokens
// - Decode JWT ID token payload (without signature verification)
// - Parse user claims from ID token
// - Save tokens to JSON file

#ifndef OIDC_TOKEN_EXCHANGE_H
#define OIDC_TOKEN_EXCHANGE_H

#include <string>

// Cấu trúc chứa thông tin token response từ OIDC token endpoint
struct OidcTokenResponse {
    // Tokens từ OIDC provider
    std::string access_token;   // Access token để gọi API
    std::string refresh_token;  // Refresh token để renew access token
    std::string id_token;       // ID token chứa user claims (JWT format)
    std::string token_type;     // Thường là "Bearer"
    int expires_in;             // Thời gian hết hạn (seconds)
    
    // Error information (nếu có)
    std::string error;
    std::string error_description;
    bool success;               // true nếu exchange thành công
    
    // User claims từ ID token (sau khi decode JWT)
    std::string email;          // Email của user
    std::string name;           // Tên đầy đủ của user
    std::string username;       // Username (preferred_username hoặc username)
    
    OidcTokenResponse() : expires_in(0), success(false) {}
};

// Exchange authorization code thành tokens
//
// Parameters:
//   tokenEndpoint - URL của OIDC token endpoint (ví dụ: https://auth.example.com/realms/realm/protocol/openid-connect/token)
//   code - Authorization code nhận được từ OIDC callback
//   redirectUri - Redirect URI đã đăng ký với OIDC provider (phải khớp với redirect_uri trong authorization request)
//   clientId - OIDC client ID
//   clientSecret - OIDC client secret (có thể empty nếu là public client)
//   verifySSL - true để verify SSL certificate, false để skip (chỉ dùng cho development)
//
// Returns:
//   OidcTokenResponse chứa tokens hoặc error information
OidcTokenResponse ExchangeOidcToken(
    const std::string& tokenEndpoint,
    const std::string& code,
    const std::string& redirectUri,
    const std::string& clientId,
    const std::string& clientSecret = "",
    bool verifySSL = true
);

// Decode JWT payload (không verify signature - chỉ decode để lấy claims)
//
// JWT format: header.payload.signature
// Function này chỉ decode payload (phần giữa) để lấy JSON claims
// Không verify signature - chỉ dùng cho development hoặc khi đã verify bằng cách khác
//
// Parameters:
//   jwt - JWT token string (format: header.payload.signature)
//
// Returns:
//   JSON payload string nếu decode thành công, empty string nếu lỗi
std::string DecodeJwtPayload(const std::string& jwt);

// Parse user claims từ ID token JSON payload
//
// Extract các thông tin: email, name, username từ JSON payload của ID token
//
// Parameters:
//   jsonPayload - JSON string từ JWT payload (sau khi decode)
//   response - OidcTokenResponse struct để fill claims vào
void ParseIdTokenClaims(const std::string& jsonPayload, OidcTokenResponse& response);

// Lưu token vào file JSON
//
// Lưu access_token, refresh_token, id_token, expiry time, và user claims vào file JSON
//
// Parameters:
//   token - OidcTokenResponse chứa tokens và claims
//   filename - Tên file để lưu (ví dụ: "token.json")
//
// Returns:
//   true nếu lưu thành công, false nếu có lỗi
bool SaveTokenToFile(const OidcTokenResponse& token, const std::string& filename);

#endif // OIDC_TOKEN_EXCHANGE_H

