// OIDC Configuration Implementation

#include "../include/oidc_config.h"
#include <fstream>
#include <cstdlib>
#include <cstring>

// Simple JSON parser để extract value
static std::string ExtractJsonValue(const std::string& json, const std::string& key) {
    if (json.empty() || key.empty()) {
        return "";
    }
    
    std::string searchKey = "\"" + key + "\"";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) {
        return "";
    }
    
    pos = json.find(":", pos);
    if (pos == std::string::npos) {
        return "";
    }
    pos++;
    
    // Bỏ qua whitespace
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) {
        pos++;
    }
    
    if (pos >= json.length()) {
        return "";
    }
    
    // String value
    if (json[pos] == '"') {
        pos++;
        size_t end = json.find('"', pos);
        if (end == std::string::npos) {
            return "";
        }
        return json.substr(pos, end - pos);
    }
    // Boolean value
    else if (json.substr(pos, 4) == "true") {
        return "true";
    }
    else if (json.substr(pos, 5) == "false") {
        return "false";
    }
    // Number value
    else if (json[pos] >= '0' && json[pos] <= '9') {
        size_t end = pos;
        while (end < json.length() && json[end] >= '0' && json[end] <= '9') {
            end++;
        }
        return json.substr(pos, end - pos);
    }
    
    return "";
}

bool LoadOidcConfigFromFile(const std::string& filename, OidcConfig& config) {
    if (filename.empty()) {
        return false;
    }
    
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    // Đọc toàn bộ file
    std::string jsonContent;
    std::string line;
    while (std::getline(file, line)) {
        jsonContent += line;
    }
    file.close();
    
    if (jsonContent.empty()) {
        return false;
    }
    
    // Parse JSON
    config.token_endpoint = ExtractJsonValue(jsonContent, "token_endpoint");
    config.redirect_uri = ExtractJsonValue(jsonContent, "redirect_uri");
    config.client_id = ExtractJsonValue(jsonContent, "client_id");
    config.client_secret = ExtractJsonValue(jsonContent, "client_secret");
    config.token_file = ExtractJsonValue(jsonContent, "token_file");
    config.listening_addr = ExtractJsonValue(jsonContent, "listening_addr");
    
    // Parse verify_ssl (boolean)
    std::string verifySSLStr = ExtractJsonValue(jsonContent, "verify_ssl");
    if (verifySSLStr == "false" || verifySSLStr == "0") {
        config.verify_ssl = false;
    } else {
        config.verify_ssl = true;
    }
    
    // Parse save_token (boolean)
    std::string saveTokenStr = ExtractJsonValue(jsonContent, "save_token");
    if (saveTokenStr == "true" || saveTokenStr == "1") {
        config.save_token = true;
    } else {
        config.save_token = false;
    }
    
    // Set defaults nếu không có
    if (config.redirect_uri.empty()) {
        config.redirect_uri = DEFAULT_REDIRECT_URI;
    }
    if (config.token_file.empty()) {
        config.token_file = DEFAULT_TOKEN_FILE;
    }
    if (config.listening_addr.empty()) {
        config.listening_addr = DEFAULT_LISTENING_ADDR;
    }
    
    // Validate: phải có token_endpoint và client_id
    return !config.token_endpoint.empty() && !config.client_id.empty();
}

bool LoadOidcConfigFromParams(
    const std::string& tokenEndpoint,
    const std::string& redirectUri,
    const std::string& clientId,
    const std::string& clientSecret,
    const std::string& tokenFile,
    int verifySSL,
    int saveToken,
    const std::string& listeningAddr,
    OidcConfig& config) {
    
    // Priority 1: Parameters
    if (!tokenEndpoint.empty()) {
        config.token_endpoint = tokenEndpoint;
    } else {
        // Priority 2: Environment variable
        const char *env = getenv("OIDC_TOKEN_ENDPOINT");
        if (env) {
            config.token_endpoint = env;
        } else {
            // Priority 3: Default
            config.token_endpoint = DEFAULT_TOKEN_ENDPOINT;
        }
    }
    
    if (!redirectUri.empty()) {
        config.redirect_uri = redirectUri;
    } else {
        const char *env = getenv("OIDC_REDIRECT_URI");
        if (env) {
            config.redirect_uri = env;
        } else {
            config.redirect_uri = DEFAULT_REDIRECT_URI;
        }
    }
    
    if (!clientId.empty()) {
        config.client_id = clientId;
    } else {
        const char *env = getenv("OIDC_CLIENT_ID");
        if (env) {
            config.client_id = env;
        } else {
            config.client_id = DEFAULT_CLIENT_ID;
        }
    }
    
    if (!clientSecret.empty()) {
        config.client_secret = clientSecret;
    } else {
        const char *env = getenv("OIDC_CLIENT_SECRET");
        if (env) {
            config.client_secret = env;
        } else if (strlen(DEFAULT_CLIENT_SECRET) > 0) {
            config.client_secret = DEFAULT_CLIENT_SECRET;
        }
    }
    
    if (!tokenFile.empty()) {
        config.token_file = tokenFile;
    } else {
        const char *env = getenv("OIDC_TOKEN_FILE");
        if (env) {
            config.token_file = env;
        } else {
            config.token_file = DEFAULT_TOKEN_FILE;
        }
    }
    
    if (verifySSL >= 0) {
        config.verify_ssl = (verifySSL != 0);
    } else {
        const char *env = getenv("OIDC_VERIFY_SSL");
        if (env) {
            config.verify_ssl = (strcmp(env, "0") != 0);
        } else {
            config.verify_ssl = DEFAULT_VERIFY_SSL;
        }
    }
    
    if (saveToken >= 0) {
        config.save_token = (saveToken != 0);
    } else {
        const char *env = getenv("OIDC_SAVE_TOKEN");
        if (env) {
            config.save_token = (strcmp(env, "0") != 0 && strcmp(env, "false") != 0);
        } else {
            config.save_token = false;  // Default: không lưu file
        }
    }
    
    if (!listeningAddr.empty()) {
        config.listening_addr = listeningAddr;
    } else {
        const char *env = getenv("OIDC_LISTENING_ADDR");
        if (env) {
            config.listening_addr = env;
        } else {
            config.listening_addr = DEFAULT_LISTENING_ADDR;
        }
    }
    
    // Validate: phải có token_endpoint và client_id
    return !config.token_endpoint.empty() && !config.client_id.empty();
}

