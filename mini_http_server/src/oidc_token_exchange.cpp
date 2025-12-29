// OIDC Token Exchange Implementation

#include "../include/oidc_token_exchange.h"

// Platform-specific HTTP client includes
#ifdef USE_MINI_CURL
    // Windows: Sử dụng mini_curl (WinHTTP)
    #include "winhttp_curl.h"
#elif defined(USE_LIBCURL)
    // Linux: Sử dụng libcurl
    #include <curl/curl.h>
    #include <map>
    #include <string>
    // Định nghĩa HttpResponse và HttpOptions cho Linux (tương tự winhttp_curl.h)
    struct HttpResponse {
        int statusCode;
        std::string body;
        std::map<std::string, std::string> headers;
        std::string error;
        HttpResponse() : statusCode(0) {}
    };
    struct HttpOptions {
        std::map<std::string, std::string> headers;
        std::string userAgent;
        int timeout;
        bool followRedirects;
        bool verifySSL;
        HttpOptions() : timeout(30), followRedirects(true), verifySSL(true) {
            userAgent = "libcurl/1.0";
        }
    };
#else
    // Fallback: thử detect tự động
    #ifdef _WIN32
        #include "winhttp_curl.h"
    #else
        #include <curl/curl.h>
        #include <map>
        #include <string>
        // Định nghĩa HttpResponse và HttpOptions cho Linux
        struct HttpResponse {
            int statusCode;
            std::string body;
            std::map<std::string, std::string> headers;
            std::string error;
            HttpResponse() : statusCode(0) {}
        };
        struct HttpOptions {
            std::map<std::string, std::string> headers;
            std::string userAgent;
            int timeout;
            bool followRedirects;
            bool verifySSL;
            HttpOptions() : timeout(30), followRedirects(true), verifySSL(true) {
                userAgent = "libcurl/1.0";
            }
        };
    #endif
#endif

#include <sstream>
#include <iomanip>
#include <cctype>
#include <fstream>
#include <algorithm>
#include <ctime>

// Simple JSON parser để extract value từ JSON string
// Hỗ trợ string và number types
//
// Parameters:
//   json - JSON string
//   key - Key cần tìm
//
// Returns:
//   Value string nếu tìm thấy, empty string nếu không tìm thấy hoặc lỗi
static std::string ExtractJsonValue(const std::string& json, const std::string& key) {
    if (json.empty() || key.empty()) {
        return "";
    }
    
    std::string searchKey = "\"" + key + "\"";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) {
        return "";
    }
    
    // Tìm dấu hai chấm sau key
    pos = json.find(":", pos);
    if (pos == std::string::npos) {
        return "";
    }
    pos++; // Bỏ qua dấu hai chấm
    
    // Bỏ qua whitespace
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) {
        pos++;
    }
    
    if (pos >= json.length()) {
        return "";
    }
    
    // Nếu là string (bắt đầu bằng ")
    if (json[pos] == '"') {
        pos++; // Bỏ qua dấu "
        size_t end = json.find('"', pos);
        if (end == std::string::npos) {
            return "";
        }
        return json.substr(pos, end - pos);
    }
    // Nếu là number
    else if (json[pos] >= '0' && json[pos] <= '9') {
        size_t end = pos;
        while (end < json.length() && json[end] >= '0' && json[end] <= '9') {
            end++;
        }
        return json.substr(pos, end - pos);
    }
    
    return "";
}

// Base64 decode (simple implementation, hỗ trợ cả URL-safe base64 cho JWT)
//
// Hỗ trợ cả standard base64 (+, /) và URL-safe base64 (-, _) như trong JWT
//
// Parameters:
//   encoded - Base64 encoded string
//
// Returns:
//   Decoded string
static std::string Base64Decode(const std::string& encoded) {
    const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string decoded;
    int val = 0, valb = -8;
    
    for (unsigned char c : encoded) {
        if (c == '=') break;
        
        size_t pos;
        if (c == '-') {
            pos = 62; // URL-safe base64: - thay cho +
        } else if (c == '_') {
            pos = 63; // URL-safe base64: _ thay cho /
        } else {
            pos = chars.find(c);
            if (pos == std::string::npos) continue;
        }
        
        val = (val << 6) + pos;
        valb += 6;
        
        if (valb >= 0) {
            decoded.push_back((val >> valb) & 0xFF);
            valb -= 8;
        }
    }
    
    return decoded;
}

// Decode JWT payload (không verify signature)
std::string DecodeJwtPayload(const std::string& jwt) {
    // JWT format: header.payload.signature
    size_t dot1 = jwt.find('.');
    if (dot1 == std::string::npos) {
        return "";
    }
    
    size_t dot2 = jwt.find('.', dot1 + 1);
    if (dot2 == std::string::npos) {
        return "";
    }
    
    // Extract payload (phần giữa 2 dấu chấm)
    std::string payloadEncoded = jwt.substr(dot1 + 1, dot2 - dot1 - 1);
    
    // Add padding nếu cần
    while (payloadEncoded.length() % 4 != 0) {
        payloadEncoded += '=';
    }
    
    // Decode base64
    std::string payload = Base64Decode(payloadEncoded);
    
    return payload;
}

// Parse claims từ ID token JSON
void ParseIdTokenClaims(const std::string& jsonPayload, OidcTokenResponse& response) {
    if (jsonPayload.empty()) {
        return;
    }
    
    response.email = ExtractJsonValue(jsonPayload, "email");
    response.name = ExtractJsonValue(jsonPayload, "name");
    response.username = ExtractJsonValue(jsonPayload, "preferred_username");
    
    // Nếu không có preferred_username, thử username
    if (response.username.empty()) {
        response.username = ExtractJsonValue(jsonPayload, "username");
    }
}

OidcTokenResponse ExchangeOidcToken(
    const std::string& tokenEndpoint,
    const std::string& code,
    const std::string& redirectUri,
    const std::string& clientId,
    const std::string& clientSecret,
    bool verifySSL) {
    
    OidcTokenResponse response;
    
    // Validate inputs
    if (tokenEndpoint.empty() || code.empty() || redirectUri.empty() || clientId.empty()) {
        response.error = "Missing required parameters";
        return response;
    }
    
    // URL encode function
    auto urlEncode = [](const std::string& str) -> std::string {
        std::ostringstream encoded;
        encoded.fill('0');
        encoded << std::hex;
        
        for (char c : str) {
            if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                encoded << c;
            } else if (c == ' ') {
                encoded << '+';
            } else {
                encoded << '%' << std::setw(2) << int((unsigned char)c);
            }
        }
        return encoded.str();
    };
    
    // Build POST data (application/x-www-form-urlencoded)
    // Format theo OAuth 2.0 Authorization Code Grant
    std::ostringstream postData;
    postData << "grant_type=authorization_code";
    postData << "&code=" << urlEncode(code);
    postData << "&redirect_uri=" << urlEncode(redirectUri);
    postData << "&client_id=" << urlEncode(clientId);
    if (!clientSecret.empty()) {
        postData << "&client_secret=" << urlEncode(clientSecret);
    }
    
    // Platform-specific HTTP client implementation
    HttpResponse httpResponse;
    
#ifdef USE_MINI_CURL
    // Windows: Sử dụng mini_curl (WinHTTP)
    WinHttpCurl client;
    if (!client.Init()) {
        response.error = "Failed to initialize HTTP client";
        response.error_description = "Cannot initialize WinHTTP session";
        return response;
    }
    
    // Set HTTP headers
    HttpOptions options;
    options.verifySSL = verifySSL;
    options.headers["Content-Type"] = "application/x-www-form-urlencoded";
    options.headers["Accept"] = "application/json";
    
    // Make POST request đến token endpoint
    httpResponse = client.Post(tokenEndpoint, postData.str(), options);
    
    // Cleanup HTTP client
    client.Cleanup();
#else
    // Linux: Sử dụng libcurl
    CURL* curl = curl_easy_init();
    if (!curl) {
        response.error = "Failed to initialize HTTP client";
        response.error_description = "Cannot initialize libcurl";
        return response;
    }
    
    // Response data structure
    struct ResponseData {
        std::string data;
        long statusCode;
    } responseData;
    responseData.statusCode = 0;
    
    // Write callback
    auto writeCallback = [](char* ptr, size_t size, size_t nmemb, void* userdata) -> size_t {
        ResponseData* rd = (ResponseData*)userdata;
        rd->data.append(ptr, size * nmemb);
        return size * nmemb;
    };
    
    // Set curl options
    curl_easy_setopt(curl, CURLOPT_URL, tokenEndpoint.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.str().c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseData);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    // SSL verification
    if (!verifySSL) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }
    
    // Set headers
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
    headers = curl_slist_append(headers, "Accept: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // Perform request
    CURLcode res = curl_easy_perform(curl);
    
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseData.statusCode);
        httpResponse.statusCode = responseData.statusCode;
        httpResponse.body = responseData.data;
    } else {
        httpResponse.statusCode = 0;
        httpResponse.error = curl_easy_strerror(res);
    }
    
    // Cleanup
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
#endif
    
    // Check HTTP response status
    if (httpResponse.statusCode != 200) {
        response.error = "HTTP " + std::to_string(httpResponse.statusCode);
        
        // Try to extract error từ response body nếu có
        if (!httpResponse.body.empty() && httpResponse.body.length() < 500) {
            std::string errorFromBody = ExtractJsonValue(httpResponse.body, "error");
            std::string errorDescFromBody = ExtractJsonValue(httpResponse.body, "error_description");
            
            if (!errorFromBody.empty()) {
                response.error = errorFromBody;
            }
            if (!errorDescFromBody.empty()) {
                response.error_description = errorDescFromBody;
            } else {
                response.error_description = httpResponse.body;
            }
        } else if (!httpResponse.error.empty()) {
            response.error_description = httpResponse.error;
        } else {
            response.error_description = "No response from server";
        }
        return response;
    }
    
    // Parse JSON response
    std::string jsonResponse = httpResponse.body;
    
    if (jsonResponse.empty()) {
        response.error = "Empty response from server";
        response.error_description = "Token endpoint returned empty response";
        return response;
    }
    
    // Check for error trong response
    std::string error = ExtractJsonValue(jsonResponse, "error");
    if (!error.empty()) {
        response.error = error;
        response.error_description = ExtractJsonValue(jsonResponse, "error_description");
        if (response.error_description.empty()) {
            response.error_description = "OIDC provider returned error";
        }
        return response;
    }
    
    // Extract tokens từ JSON response
    response.access_token = ExtractJsonValue(jsonResponse, "access_token");
    response.refresh_token = ExtractJsonValue(jsonResponse, "refresh_token");
    response.id_token = ExtractJsonValue(jsonResponse, "id_token");
    response.token_type = ExtractJsonValue(jsonResponse, "token_type");
    
    // Extract expires_in (thời gian hết hạn tính bằng seconds)
    std::string expiresInStr = ExtractJsonValue(jsonResponse, "expires_in");
    if (!expiresInStr.empty()) {
        try {
            response.expires_in = std::stoi(expiresInStr);
        } catch (const std::exception&) {
            // Nếu không parse được, set default 3600 seconds
            response.expires_in = 3600;
        }
    }
    
    // Decode ID token và parse user claims
    if (!response.id_token.empty()) {
        std::string idTokenPayload = DecodeJwtPayload(response.id_token);
        if (!idTokenPayload.empty()) {
            ParseIdTokenClaims(idTokenPayload, response);
        }
    }
    
    // Validate: Phải có access_token để coi là thành công
    if (!response.access_token.empty()) {
        response.success = true;
    } else {
        response.error = "No access_token in response";
        response.error_description = "Token endpoint response does not contain access_token";
    }
    
    return response;
}

// Lưu token vào file JSON
bool SaveTokenToFile(const OidcTokenResponse& token, const std::string& filename) {
    if (filename.empty()) {
        return false;
    }
    
    if (!token.success) {
        return false;
    }
    
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    // Calculate expiry time (Unix timestamp)
    time_t now = time(NULL);
    time_t expiry = now + token.expires_in;
    
    // Write JSON format
    file << "{\n";
    file << "  \"access_token\": \"" << token.access_token << "\",\n";
    
    if (!token.refresh_token.empty()) {
        file << "  \"refresh_token\": \"" << token.refresh_token << "\",\n";
    }
    
    if (!token.token_type.empty()) {
        file << "  \"token_type\": \"" << token.token_type << "\",\n";
    }
    
    file << "  \"expiry\": " << expiry << ",\n";
    
    if (!token.id_token.empty()) {
        file << "  \"id_token\": \"" << token.id_token << "\",\n";
    }
    
    if (!token.email.empty()) {
        file << "  \"email\": \"" << token.email << "\",\n";
    }
    
    if (!token.name.empty()) {
        file << "  \"name\": \"" << token.name << "\",\n";
    }
    
    if (!token.username.empty()) {
        file << "  \"username\": \"" << token.username << "\"\n";
    }
    
    file << "}\n";
    
    file.close();
    return file.good();
}

