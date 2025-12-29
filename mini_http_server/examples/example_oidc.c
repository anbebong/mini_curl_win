// OIDC Callback Server Example - C
//
// Ví dụ về cách sử dụng mini_http_server library với OIDC callback từ C code
//
// Build: cmake --build build
// Run: .\build\bin\example_oidc.exe [config_file]
//   hoặc: .\build\bin\example_oidc.exe [listening_addr] [token_endpoint] [client_id] [client_secret] [redirect_uri]

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mini_http_server.h"

// Token callback - được gọi khi token exchange thành công
void on_token_received(
    const char* access_token,
    const char* refresh_token,
    const char* id_token,
    const char* email,
    const char* name,
    const char* username,
    void* user_data
) {
    printf("\n=== Token Received ===\n");
    printf("Access Token: %s\n", access_token);
    if (refresh_token) {
        printf("Refresh Token: %s\n", refresh_token);
    }
    if (id_token) {
        printf("ID Token: %s\n", id_token);
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
}

// Error callback - được gọi khi có lỗi
void on_error(
    const char* error,
    const char* error_description,
    void* user_data
) {
    printf("\n=== Error ===\n");
    printf("Error: %s\n", error);
    if (error_description) {
        printf("Description: %s\n", error_description);
    }
    printf("=============\n\n");
}

int main(int argc, char *argv[]) {
    MiniHttpServerConfig* config = MiniHttpServerConfig_New();
    if (!config) {
        fprintf(stderr, "Failed to create config\n");
        return 1;
    }
    
    bool configLoaded = false;
    
    // Try to load from config file
    if (argc == 2) {
        const char* arg1 = argv[1];
        // Nếu không phải là URL, coi như là config file
        if (strstr(arg1, "http://") != arg1 && strstr(arg1, "https://") != arg1) {
            if (MiniHttpServerConfig_LoadFromFile(config, arg1)) {
                configLoaded = true;
                printf("Configuration loaded from file: %s\n", arg1);
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
        const char* tokenFile = (argc > 6) ? argv[6] : NULL;
        int verifySSL = (argc > 7) ? (strcmp(argv[7], "0") == 0 ? 0 : 1) : -1;
        int saveToken = (argc > 8) ? (strcmp(argv[8], "0") == 0 ? 0 : 1) : -1;
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
            fprintf(stderr, "Usage: %s [config_file]\n", argv[0]);
            fprintf(stderr, "   or: %s [listening_addr] [token_endpoint] [client_id] [client_secret] [redirect_uri]\n", argv[0]);
            MiniHttpServerConfig_Free(config);
            return 1;
        }
    }
    
    printf("\nOIDC Configuration:\n");
    printf("  Listening Address: %s\n", config->listening_addr);
    printf("  Redirect URI: %s\n", config->redirect_uri ? config->redirect_uri : "(not set)");
    printf("  Token Endpoint: %s\n", config->token_endpoint ? config->token_endpoint : "(not set)");
    printf("  Client ID: %s\n", config->client_id ? config->client_id : "(not set)");
    printf("  Verify SSL: %s\n", config->verify_ssl ? "Yes" : "No");
    printf("  Save Token: %s\n", config->save_token ? "Yes" : "No");
    printf("\n");
    
    // Start server với callbacks
    int result = MiniHttpServer_Start(
        config,
        on_token_received,  // Token callback
        on_error,           // Error callback
        config              // User data (pass config pointer)
    );
    
    MiniHttpServerConfig_Free(config);
    
    if (result != 0) {
        fprintf(stderr, "Failed to start server\n");
        return 1;
    }
    
    return 0;
}

