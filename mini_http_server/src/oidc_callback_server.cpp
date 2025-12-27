// HTTP Server để lắng nghe OIDC callback sử dụng Mongoose
//
// Server này lắng nghe OIDC callback từ OIDC provider, nhận authorization code,
// và tự động exchange code thành tokens (access token, refresh token, ID token).
//
// Build: cmake --build build
// Run: .\build\bin\oidc_callback_server.exe
//
// Configuration:
//   - Có thể set environment variables hoặc chỉnh sửa DEFAULT_* values trong code
//   - Required: OIDC_TOKEN_ENDPOINT, OIDC_CLIENT_ID
//   - Optional: OIDC_REDIRECT_URI, OIDC_CLIENT_SECRET, OIDC_TOKEN_FILE, OIDC_VERIFY_SSL

#include <signal.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sstream>
#include "mongoose.h"
#include "../include/oidc_token_exchange.h"
#include "../include/oidc_config.h"

// Global OIDC configuration
static OidcConfig s_oidc_config;
static int s_signo = 0;

// Xử lý signal để dừng server gracefully
// Khi nhận SIGINT (Ctrl+C) hoặc SIGTERM, set flag để dừng event loop
static void signal_handler(int signo) {
    s_signo = signo;
}

// Callback function để xử lý HTTP requests từ Mongoose
// Xử lý các endpoints:
//   - /callback: OIDC callback endpoint (nhận authorization code)
//   - /health: Health check endpoint
//   - /: Root endpoint (welcome page)
//
// OIDC callback thường có dạng: /callback?code=xxx&state=xxx
static void oidc_callback_handler(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *) ev_data;
        
        // Xử lý OIDC callback endpoint
        if (mg_match(hm->uri, mg_str("/callback"), NULL)) {
            // Lấy query parameters
            char code[512] = {0};
            char state[512] = {0};
            char error[512] = {0};
            char error_description[512] = {0};
            
            // Parse query parameters
            mg_http_get_var(&hm->query, "code", code, sizeof(code));
            mg_http_get_var(&hm->query, "state", state, sizeof(state));
            mg_http_get_var(&hm->query, "error", error, sizeof(error));
            mg_http_get_var(&hm->query, "error_description", error_description, sizeof(error_description));
            
            // Log thông tin callback
            printf("\n=== OIDC Callback Received ===\n");
            printf("URI: %.*s\n", (int)hm->uri.len, hm->uri.buf);
            printf("Query: %.*s\n", (int)hm->query.len, hm->query.buf);
            
            if (strlen(error) > 0) {
                // Có lỗi từ OIDC provider
                printf("Error: %s\n", error);
                if (strlen(error_description) > 0) {
                    printf("Error Description: %s\n", error_description);
                }
                
                // Trả về HTML response với thông báo lỗi
                if (strlen(error_description) > 0) {
                    mg_http_reply(c, 200, "Content-Type: text/html\r\n",
                        "<!DOCTYPE html>\n"
                        "<html><head><title>OIDC Callback - Error</title></head>\n"
                        "<body>\n"
                        "<h1>OIDC Authentication Error</h1>\n"
                        "<p><strong>Error:</strong> %s</p>\n"
                        "<p><strong>Error Description:</strong> %s</p>\n"
                        "</body></html>\n",
                        error, error_description);
                } else {
                    mg_http_reply(c, 200, "Content-Type: text/html\r\n",
                        "<!DOCTYPE html>\n"
                        "<html><head><title>OIDC Callback - Error</title></head>\n"
                        "<body>\n"
                        "<h1>OIDC Authentication Error</h1>\n"
                        "<p><strong>Error:</strong> %s</p>\n"
                        "</body></html>\n",
                        error);
                }
            } else if (strlen(code) > 0) {
                // Thành công - có authorization code từ OIDC provider
                printf("Authorization Code: %s\n", code);
                if (strlen(state) > 0) {
                    printf("State: %s\n", state);
                }
                
                // Exchange authorization code thành tokens nếu đã cấu hình
                bool tokenExchangeSuccess = false;
                std::string tokenInfo;
                
                // Kiểm tra xem đã cấu hình đủ thông tin chưa
                if (!s_oidc_config.token_endpoint.empty() && !s_oidc_config.client_id.empty()) {
                    printf("\nExchanging authorization code for token...\n");
                    printf("Token Endpoint: %s\n", s_oidc_config.token_endpoint.c_str());
                    printf("Redirect URI: %s\n", s_oidc_config.redirect_uri.c_str());
                    
                    // Exchange code thành tokens
                    OidcTokenResponse response = ExchangeOidcToken(
                        s_oidc_config.token_endpoint,
                        std::string(code),
                        s_oidc_config.redirect_uri,
                        s_oidc_config.client_id,
                        s_oidc_config.client_secret,
                        s_oidc_config.verify_ssl
                    );
                    
                    if (response.success) {
                        printf("Token exchange successful!\n");
                        printf("Access Token: %s\n", response.access_token.c_str());
                        if (!response.refresh_token.empty()) {
                            printf("Refresh Token: %s\n", response.refresh_token.c_str());
                        }
                        if (!response.id_token.empty()) {
                            printf("ID Token: %s\n", response.id_token.c_str());
                        }
                        if (!response.email.empty()) {
                            printf("Email: %s\n", response.email.c_str());
                        }
                        if (!response.name.empty()) {
                            printf("Name: %s\n", response.name.c_str());
                        }
                        if (!response.username.empty()) {
                            printf("Username: %s\n", response.username.c_str());
                        }
                        
                        // Chỉ lưu vào file nếu được cấu hình
                        if (s_oidc_config.save_token && !s_oidc_config.token_file.empty()) {
                            if (SaveTokenToFile(response, s_oidc_config.token_file)) {
                                printf("Tokens saved to: %s\n", s_oidc_config.token_file.c_str());
                            } else {
                                printf("Warning: Failed to save tokens to file: %s\n", s_oidc_config.token_file.c_str());
                            }
                        } else {
                            printf("Note: Tokens are not saved to file (use --save-token or set save_token=true in config)\n");
                        }
                        
                        std::ostringstream oss;
                        oss << "Success! Token exchanged successfully\n";
                        oss << "Access Token: " << response.access_token << "\n";
                        if (!response.email.empty()) {
                            oss << "Email: " << response.email << "\n";
                        }
                        if (!response.name.empty()) {
                            oss << "Name: " << response.name << "\n";
                        }
                        if (!response.username.empty()) {
                            oss << "Username: " << response.username << "\n";
                        }
                        if (s_oidc_config.save_token && !s_oidc_config.token_file.empty()) {
                            oss << "Tokens saved to: " << s_oidc_config.token_file << "\n";
                        }
                        tokenInfo = oss.str();
                        tokenExchangeSuccess = true;
                    } else {
                        printf("Token exchange failed:\n");
                        printf("Error: %s\n", response.error.c_str());
                        if (!response.error_description.empty()) {
                            printf("Error Description: %s\n", response.error_description.c_str());
                        }
                        
                        std::ostringstream oss;
                        oss << "Error: " << response.error;
                        if (!response.error_description.empty()) {
                            oss << " - " << response.error_description;
                        }
                        tokenInfo = oss.str();
                    }
                } else {
                    printf("Token exchange skipped (not configured)\n");
                    tokenInfo = "Token exchange not configured. Please provide token_endpoint and client_id in config file or parameters.";
                }
                
                // HTML response với success message (giống Go code)
                const char* successHtml = 
                    "<!DOCTYPE html>\n"
                    "<html lang=\"en\">\n"
                    "<head>\n"
                    "    <meta content=\"width=device-width, initial-scale=1\" name=\"viewport\"/>\n"
                    "    <style>\n"
                    "        body {\n"
                    "            display: flex;\n"
                    "            justify-content: center;\n"
                    "            align-items: center;\n"
                    "            min-height: 100vh;\n"
                    "            margin: 0;\n"
                    "            background: #f7f8f9;\n"
                    "            font-family: sans-serif, Arial, Tahoma;\n"
                    "        }\n"
                    "        .container {\n"
                    "            width: 100%%;\n"
                    "            background: white;\n"
                    "            border: 1px solid #e8e9ea;\n"
                    "            text-align: center;\n"
                    "            padding: 20px;\n"
                    "            padding-bottom: 50px;\n"
                    "            max-width: 550px;\n"
                    "            margin: 0 10px;\n"
                    "        }\n"
                    "        .content {\n"
                    "            font-size: 13px;\n"
                    "            color: #525252;\n"
                    "            line-height: 18px;\n"
                    "            padding: 10px 0;\n"
                    "        }\n"
                    "        .content div {\n"
                    "            font-size: 18px;\n"
                    "            line-height: normal;\n"
                    "            margin-bottom: 5px;\n"
                    "            color: black;\n"
                    "        }\n"
                    "    </style>\n"
                    "    <title>Login Successful</title>\n"
                    "</head>\n"
                    "<body>\n"
                    "<div class=\"container\">\n"
                    "    <div class=\"content\">\n"
                    "        <div>Login successful</div>\n"
                    "        Your device is now registered and logged in.\n"
                    "        <br>\n"
                    "        You can close this window.\n"
                    "    </div>\n"
                    "</div>\n"
                    "</body>\n"
                    "</html>\n";
                
                if (tokenExchangeSuccess) {
                    mg_http_reply(c, 200, "Content-Type: text/html\r\n", "%s", successHtml);
                } else {
                    mg_http_reply(c, 200, "Content-Type: text/html\r\n",
                        "<!DOCTYPE html>\n"
                        "<html><head><title>OIDC Callback - Success</title></head>\n"
                        "<body>\n"
                        "<h1>OIDC Authentication Successful</h1>\n"
                        "<p>Authorization code received successfully!</p>\n"
                        "<p><strong>Code:</strong> %s</p>\n"
                        "<p><strong>Note:</strong> %s</p>\n"
                        "<p>You can close this window now.</p>\n"
                        "</body></html>\n",
                        code, tokenInfo.c_str());
                }
            } else {
                // Không có code và không có error - có thể là request không hợp lệ
                printf("Warning: No code or error parameter found\n");
                mg_http_reply(c, 400, "Content-Type: text/html\r\n",
                    "<!DOCTYPE html>\n"
                    "<html><head><title>OIDC Callback - Invalid Request</title></head>\n"
                    "<body>\n"
                    "<h1>Invalid OIDC Callback</h1>\n"
                    "<p>No authorization code or error parameter found in the callback.</p>\n"
                    "</body></html>\n");
            }
            
            printf("================================\n\n");
        }
        // Health check endpoint
        else if (mg_match(hm->uri, mg_str("/health"), NULL)) {
            mg_http_reply(c, 200, "Content-Type: application/json\r\n",
                "{\"status\":\"ok\",\"service\":\"oidc-callback-server\"}\n");
        }
        // Root endpoint
        else if (mg_match(hm->uri, mg_str("/"), NULL)) {
            mg_http_reply(c, 200, "Content-Type: text/html\r\n",
                "<!DOCTYPE html>\n"
                "<html><head><title>OIDC Callback Server</title></head>\n"
                "<body>\n"
                "<h1>OIDC Callback Server</h1>\n"
                "<p>Server is running and ready to receive OIDC callbacks.</p>\n"
                "<p>Callback URL: <code>http://localhost:8080/callback</code></p>\n"
                "<p><a href=\"/health\">Health Check</a></p>\n"
                "</body></html>\n");
        }
        // 404 cho các endpoint khác
        else {
            mg_http_reply(c, 404, "Content-Type: text/html\r\n",
                "<!DOCTYPE html>\n"
                "<html><head><title>404 Not Found</title></head>\n"
                "<body><h1>404 Not Found</h1></body></html>\n");
        }
    }
}

// Parse command line arguments và load configuration
// Returns: true nếu load thành công, false nếu có lỗi
static bool LoadConfiguration(int argc, char *argv[]) {
    // Try to load from config file first (nếu có 1 argument và là file path)
    if (argc == 2) {
        std::string arg1 = argv[1];
        // Nếu không phải là URL (bắt đầu bằng http:// hoặc https://), coi như là config file
        if (arg1.find("http://") != 0 && arg1.find("https://") != 0) {
            if (LoadOidcConfigFromFile(arg1, s_oidc_config)) {
                printf("Configuration loaded from file: %s\n", arg1.c_str());
                return true;
            } else {
                fprintf(stderr, "Warning: Failed to load config from file: %s\n", arg1.c_str());
                fprintf(stderr, "Falling back to parameters/environment variables...\n");
            }
        }
    }
    
    // Load từ parameters/environment variables
    std::string tokenEndpoint = (argc > 2) ? argv[2] : "";
    std::string clientId = (argc > 3) ? argv[3] : "";
    std::string clientSecret = (argc > 4) ? argv[4] : "";
    std::string redirectUri = (argc > 5) ? argv[5] : "";
    std::string tokenFile = (argc > 6) ? argv[6] : "";
    int verifySSL = (argc > 7) ? (strcmp(argv[7], "0") == 0 ? 0 : 1) : -1;
    int saveToken = (argc > 8) ? (strcmp(argv[8], "0") == 0 ? 0 : 1) : -1;
    std::string listeningAddr = (argc > 1) ? argv[1] : "";
    
    if (!LoadOidcConfigFromParams(tokenEndpoint, redirectUri, clientId, clientSecret, 
                                 tokenFile, verifySSL, saveToken, listeningAddr, s_oidc_config)) {
        fprintf(stderr, "Error: Missing required configuration (token_endpoint and client_id)\n");
        return false;
    }
    
    return true;
}

// Print usage information
static void PrintUsage(const char *program_name) {
    printf("Usage:\n");
    printf("  %s [config_file]\n", program_name);
    printf("  %s [listening_addr] [token_endpoint] [client_id] [client_secret] [redirect_uri] [token_file] [verify_ssl] [save_token]\n", program_name);
    printf("\n");
    printf("Examples:\n");
    printf("  %s config.json\n", program_name);
    printf("  %s http://localhost:8085 https://auth.example.com/token client-id client-secret http://localhost:8085/callback\n", program_name);
    printf("\n");
}

// Print server information
static void PrintServerInfo() {
    printf("OIDC Callback Server started\n");
    printf("Listening on: %s\n", s_oidc_config.listening_addr.c_str());
    printf("Callback URL: %s/callback\n", s_oidc_config.listening_addr.c_str());
    printf("Health check: %s/health\n", s_oidc_config.listening_addr.c_str());
    
    printf("\nOIDC Configuration:\n");
    printf("  Listening Address: %s\n", s_oidc_config.listening_addr.c_str());
    printf("  Redirect URI: %s\n", s_oidc_config.redirect_uri.c_str());
    printf("  Verify SSL: %s\n", s_oidc_config.verify_ssl ? "Yes" : "No");
    printf("  Save Token to File: %s\n", s_oidc_config.save_token ? "Yes" : "No (only console output)");
    if (s_oidc_config.save_token && !s_oidc_config.token_file.empty()) {
        printf("  Token File: %s\n", s_oidc_config.token_file.c_str());
    }
    
    printf("\nOIDC Token Exchange: Configured\n");
    printf("  Token Endpoint: %s\n", s_oidc_config.token_endpoint.c_str());
    printf("  Client ID: %s\n", s_oidc_config.client_id.c_str());
    printf("  Client Secret: %s\n", s_oidc_config.client_secret.empty() ? "(not set - public client)" : "***");
}

// Initialize và start HTTP server
static bool StartServer() {
    struct mg_mgr mgr;
    struct mg_connection *c;
    
    // Initialize Mongoose event manager
    mg_mgr_init(&mgr);
    
    // Create listening connection
    c = mg_http_listen(&mgr, s_oidc_config.listening_addr.c_str(), oidc_callback_handler, NULL);
    if (c == NULL) {
        fprintf(stderr, "Error: Cannot start server on %s\n", s_oidc_config.listening_addr.c_str());
        return false;
    }
    
    PrintServerInfo();
    printf("\nPress Ctrl+C to stop...\n\n");
    
    // Event loop
    while (s_signo == 0) {
        mg_mgr_poll(&mgr, 1000);  // 1 second timeout
    }
    
    // Cleanup
    printf("\nShutting down server...\n");
    mg_mgr_free(&mgr);
    printf("Server stopped.\n");
    
    return true;
}

int main(int argc, char *argv[]) {
    // Print usage nếu có --help hoặc -h
    if (argc > 1 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        PrintUsage(argv[0]);
        return 0;
    }
    
    // Load configuration
    if (!LoadConfiguration(argc, argv)) {
        PrintUsage(argv[0]);
        return 1;
    }
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Start server
    if (!StartServer()) {
        return 1;
    }
    
    return 0;
}

